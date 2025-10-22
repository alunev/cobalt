// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/crash/minidump_parser.h"

#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/memory_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/snapshot/thread_snapshot.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"

namespace cobalt {
namespace android {

namespace {

// Simple delegate to read memory into a vector.
class VectorMemoryDelegate : public crashpad::MemorySnapshot::Delegate {
 public:
  explicit VectorMemoryDelegate(std::vector<uint8_t>* buffer)
      : buffer_(buffer) {}

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    if (size > 0) {
      buffer_->resize(size);
      memcpy(buffer_->data(), data, size);
    }
    return true;
  }

 private:
  std::vector<uint8_t>* buffer_;
};

// Helper to safely read a value from the stack buffer.
template <typename T>
bool SafeRead(const std::vector<uint8_t>& stack_memory,
              uint64_t stack_base_addr,
              uint64_t target_addr,
              T* out_value) {
  if (target_addr < stack_base_addr) {
    return false;
  }
  size_t offset = target_addr - stack_base_addr;
  // Check if reading sizeof(T) bytes would go past the end of the buffer.
  if (offset >= stack_memory.size() ||
      stack_memory.size() - offset < sizeof(T)) {
    return false;
  }

  memcpy(out_value, &stack_memory[offset], sizeof(T));
  return true;
}

std::string WalkStack(const crashpad::ThreadSnapshot* thread) {
  std::stringstream ss;
  const crashpad::CPUContext* context = thread->Context();
  const crashpad::MemorySnapshot* stack = thread->Stack();

  if (!context || !stack) {
    return "Context or Stack missing";
  }

  // Read the entire stack memory for this thread.
  std::vector<uint8_t> stack_memory;
  VectorMemoryDelegate delegate(&stack_memory);
  if (!stack->Read(&delegate)) {
    return "Failed to read stack memory";
  }

  uint64_t stack_base = stack->Address();
  uint64_t current_pc = 0;
  uint64_t current_fp = 0;
  uint64_t stack_top = stack_base + stack_memory.size();
  int pointer_width = 8;  // Default to 64-bit

  // Initialize PC and FP based on architecture
  if (context->architecture == crashpad::kCPUArchitectureARM64) {
    current_pc = context->arm64->pc;
    // FP is x29 in ARM64
    current_fp = context->arm64->regs[29];
    pointer_width = 8;
  } else if (context->architecture == crashpad::kCPUArchitectureX86) {
    current_pc = context->x86->eip;
    current_fp = context->x86->ebp;
    pointer_width = 4;
  } else if (context->architecture == crashpad::kCPUArchitectureX86_64) {
    current_pc = context->x86_64->rip;
    current_fp = context->x86_64->rbp;
    pointer_width = 8;
  } else if (context->architecture == crashpad::kCPUArchitectureARM) {
    current_pc = context->arm->pc;
    current_fp = context->arm->fp;
    pointer_width = 4;
  } else {
    return "Unsupported architecture for walking";
  }

  ss << "Thread ID: " << thread->ThreadID() << "\n";

  int depth = 0;
  const int kMaxDepth = 64;

  while (depth < kMaxDepth) {
    // Print current frame
    ss << "#" << std::setfill('0') << std::setw(2) << depth << " pc "
       << std::setw(pointer_width * 2) << std::hex << current_pc << "\n";

    if (current_fp < stack_base || current_fp >= stack_top) {
      break;
    }

    // Read next FP and PC (Return Address)
    uint64_t next_fp = 0;
    uint64_t next_pc = 0;

    if (pointer_width == 8) {
      uint64_t val_fp = 0;
      uint64_t val_pc = 0;
      // ARM64 standard: FP points to previous FP. Return address is at FP+8.
      // X86_64 standard: RBP points to previous RBP. Return address is at
      // RBP+8.
      if (!SafeRead(stack_memory, stack_base, current_fp, &val_fp) ||
          !SafeRead(stack_memory, stack_base, current_fp + 8, &val_pc)) {
        break;
      }
      next_fp = val_fp;
      next_pc = val_pc;
    } else {
      uint32_t val_fp = 0;
      uint32_t val_pc = 0;
      // We assume standard [FP] = prev_FP, [FP+4] = RA
      if (!SafeRead(stack_memory, stack_base, current_fp, &val_fp) ||
          !SafeRead(stack_memory, stack_base, current_fp + 4, &val_pc)) {
        break;
      }
      next_fp = val_fp;
      next_pc = val_pc;
    }

    // Sanity check: Stack grows down, so next FP should be > current FP
    if (next_fp <= current_fp) {
      break;
    }

    current_fp = next_fp;
    current_pc = next_pc;

    if (current_pc == 0) {
      break;
    }

    depth++;
  }
  return ss.str();
}

}  // namespace

std::vector<std::string> ParseMinidump(const std::string& minidump_path) {
  std::vector<std::string> stack_traces;
  crashpad::FileReader file_reader;
  if (!file_reader.Open(base::FilePath(minidump_path))) {
    LOG(ERROR) << "Failed to open minidump file: " << minidump_path;
    return stack_traces;
  }

  crashpad::ProcessSnapshotMinidump snapshot;
  if (!snapshot.Initialize(&file_reader)) {
    LOG(ERROR) << "Failed to initialize ProcessSnapshotMinidump";
    return stack_traces;
  }

  // 1. Get the list of hung thread IDs from crash keys
  std::string hung_thread_ids_str;
  LOG(ERROR) << "Searching for 'list-of-hung-threads' annotation...";

  // Check process-level annotations
  const auto& process_annotations = snapshot.AnnotationsSimpleMap();
  LOG(ERROR) << "Process annotations size: " << process_annotations.size();
  for (const auto& pair : process_annotations) {
    LOG(ERROR) << "  " << pair.first << " = " << pair.second;
  }
  auto it = process_annotations.find("list-of-hung-threads");
  if (it != process_annotations.end()) {
    hung_thread_ids_str = it->second;
    LOG(ERROR) << "Found in process annotations: " << hung_thread_ids_str;
  } else {
    // Check module-level annotations
    LOG(ERROR) << "Not found in process annotations, checking modules...";
    for (const auto* module : snapshot.Modules()) {
      LOG(ERROR) << "Module: " << module->Name();
      const auto& module_annotations = module->AnnotationsSimpleMap();
      LOG(ERROR) << "  Module annotations size: " << module_annotations.size();
      for (const auto& pair : module_annotations) {
        LOG(ERROR) << "    " << pair.first << " = " << pair.second;
        if (pair.first == "list-of-hung-threads") {
          hung_thread_ids_str = pair.second;
          LOG(ERROR) << "Found in module " << module->Name() << ": "
                     << hung_thread_ids_str;
          break;
        }
      }

      if (!hung_thread_ids_str.empty()) {
        break;
      }

      // Check Crashpad-specific annotations from AnnotationObjects
      const auto& annotation_objects = module->AnnotationObjects();
      LOG(ERROR) << "  Module AnnotationObjects size: "
                 << annotation_objects.size();
      for (const auto& annotation : annotation_objects) {
        std::string value_str(annotation.value.begin(), annotation.value.end());
        LOG(ERROR) << "    " << annotation.name << " = " << value_str;
        if (annotation.name == "list-of-hung-threads") {
          hung_thread_ids_str = value_str;
          LOG(ERROR) << "Found in module AnnotationObjects " << module->Name()
                     << ": " << hung_thread_ids_str;
          break;
        }
      }
      if (!hung_thread_ids_str.empty()) {
        break;
      }
    }
  }

  if (hung_thread_ids_str.empty()) {
    LOG(ERROR)
        << "Minidump does not contain 'list-of-hung-threads' annotation.";
    return stack_traces;
  }

  LOG(ERROR) << "Raw hung thread IDs string: " << hung_thread_ids_str;

  // Split by '|'
  std::vector<std::string> hung_thread_id_tokens =
      base::SplitString(hung_thread_ids_str, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  std::vector<uint64_t> hung_thread_ids;
  for (const auto& token : hung_thread_id_tokens) {
    uint64_t tid;
    if (base::StringToUint64(token, &tid)) {
      hung_thread_ids.push_back(tid);
      LOG(ERROR) << "Parsed hung thread ID: " << tid;
    } else {
      LOG(ERROR) << "Failed to parse thread ID token: " << token;
    }
  }

  if (hung_thread_ids.empty()) {
    LOG(ERROR) << "No valid hung thread IDs found after parsing.";
    return stack_traces;
  }

  // 2. Find and walk stacks for those threads
  LOG(ERROR) << "Searching for threads in snapshot...";
  for (const auto* thread : snapshot.Threads()) {
    LOG(ERROR) << "  Snapshot Thread ID: " << thread->ThreadID();
    for (uint64_t hung_tid : hung_thread_ids) {
      if (thread->ThreadID() == hung_tid) {
        LOG(ERROR) << "    MATCH! Walking stack for TID " << hung_tid;
        stack_traces.push_back(WalkStack(thread));
        break;
      }
    }
  }

  if (stack_traces.empty()) {
    LOG(ERROR) << "No stacks generated for the hung thread IDs.";
  }

  return stack_traces;
}

}  // namespace android
}  // namespace cobalt
