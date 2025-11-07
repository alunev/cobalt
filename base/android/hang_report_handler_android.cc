// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/hang_report_handler_android.h"

#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <stdio.h>  // For dprintf
#include <string.h>
#include <sys/prctl.h>  // For prctl
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/threading/hang_watcher.h"

namespace base {
namespace android {

namespace {

#define HW_LOG(message) LOG(ERROR) << "HangWatcher DEBUG: " << message

class AndroidHangReportHandler : public base::HangWatcher::HangReportHandler {
 public:
  AndroidHangReportHandler() = default;
  ~AndroidHangReportHandler() override = default;

  void OnHangDetected(const std::string& thread_type_name,
                      PlatformThreadId thread_id,
                      int64_t hang_duration_ms) override {
    HW_LOG(
        "!!!!!!!!!!!!!!!!!!!!!!!! HANG DETECTED !!!!!!!!!!!!!!!!!!!!!!!! TID: "
        << thread_id);
    HW_LOG(
        "AndroidHangReportHandler::OnHangDetected - ENTRY, TID: " << thread_id);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
      HW_LOG("Pipe creation failed: " << strerror(errno));
      return;
    }

    // Attempt to make the process dumpable, may help ptrace.
    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
      HW_LOG("Parent: prctl(PR_SET_DUMPABLE, 1) failed: " << strerror(errno));
    } else {
      HW_LOG("Parent: prctl(PR_SET_DUMPABLE, 1) succeeded.");
    }

    pid_t child_pid = fork();

    if (child_pid < 0) {
      HW_LOG("Fork failed: " << strerror(errno));
      return;
    } else if (child_pid == 0) {
      // Child process
      HW_LOG("Child: process created, PID: " << getpid());
      close(pipefd[0]);  // Close unused read end

      if (ptrace(PTRACE_ATTACH, thread_id, NULL, NULL) == -1) {
        HW_LOG("Child: PTRACE_ATTACH failed for TID " << thread_id << ": "
                                                      << strerror(errno));
        _exit(1);
      }
      HW_LOG("Child: PTRACE_ATTACH successful for TID " << thread_id);

      int status;
      if (waitpid(thread_id, &status, 0) == -1) {
        HW_LOG("Child: waitpid failed for TID " << thread_id << ": "
                                                << strerror(errno));
        ptrace(PTRACE_DETACH, thread_id, NULL, NULL);
        _exit(1);
      }

      if (WIFSTOPPED(status)) {
        HW_LOG("Child: Thread " << thread_id << " stopped");
#if defined(__i386__)
        struct user_regs_struct ptrace_regs;
        if (ptrace(PTRACE_GETREGS, thread_id, NULL, &ptrace_regs) != -1) {
          HW_LOG("Child: Registers for TID " << thread_id << ":");
          HW_LOG("  eip: " << std::hex << ptrace_regs.eip);
          HW_LOG("  esp: " << std::hex << ptrace_regs.esp);
          HW_LOG("  ebp: " << std::hex << ptrace_regs.ebp);

          // Manual stack walk
          std::stringstream stack_trace_stream;
          stack_trace_stream << "Stack Trace (TID: " << thread_id << "):\n";
          stack_trace_stream << "  0x" << std::hex << ptrace_regs.eip << "\n";

          uintptr_t bp = ptrace_regs.ebp;
          for (int i = 0; i < 64 && bp != 0; ++i) {
            errno = 0;
            uintptr_t return_addr = ptrace(PTRACE_PEEKDATA, thread_id,
                                           (void*)(bp + sizeof(void*)), NULL);
            if (errno != 0) {
              HW_LOG("Child: PTRACE_PEEKDATA failed for return address at "
                     << std::hex << (bp + sizeof(void*)) << ": "
                     << strerror(errno));
              break;
            }
            if (return_addr == 0) {
              break;
            }
            stack_trace_stream << "  0x" << std::hex << return_addr << "\n";

            errno = 0;
            uintptr_t next_bp =
                ptrace(PTRACE_PEEKDATA, thread_id, (void*)bp, NULL);
            if (errno != 0) {
              HW_LOG("Child: PTRACE_PEEKDATA failed for next ebp at "
                     << std::hex << bp << ": " << strerror(errno));
              break;
            }
            if (next_bp <= bp) {
              HW_LOG("Child: Stack walk terminated to prevent loop: next_bp "
                     << std::hex << next_bp << " <= bp " << bp);
              break;
            }
            bp = next_bp;
          }

          HW_LOG("Child: Stack trace captured:\n" << stack_trace_stream.str());

          // Write stack trace to pipe
          std::string stack_trace_str = stack_trace_stream.str();
          if (write(pipefd[1], stack_trace_str.c_str(),
                    stack_trace_str.length()) == -1) {
            HW_LOG("Child: Failed to write stack trace to pipe: "
                   << strerror(errno));
          } else {
            HW_LOG("Child: Stack trace written to pipe.");
          }
        } else {
          HW_LOG("Child: PTRACE_GETREGS failed for TID " << thread_id << ": "
                                                         << strerror(errno));
        }
#elif defined(__aarch64__)
        struct user_pt_regs ptrace_regs;
        if (ptrace(PTRACE_GETREGS, thread_id, NULL, &ptrace_regs) != -1) {
          HW_LOG("Child: Registers for TID " << thread_id << ":");
          HW_LOG("  pc: " << std::hex << ptrace_regs.pc);
          HW_LOG("  sp: " << std::hex << ptrace_regs.sp);
          uintptr_t bp = ptrace_regs.regs[29];  // Frame pointer (x29)
          HW_LOG("  fp: " << std::hex << bp);

          // Manual stack walk for ARM64
          std::stringstream stack_trace_stream;
          stack_trace_stream << "Stack Trace (TID: " << thread_id << "):\\n";
          stack_trace_stream << "  0x" << std::hex << ptrace_regs.pc << "\\n";

          for (int i = 0; i < 64 && bp != 0; ++i) {
            errno = 0;
            uintptr_t return_addr = ptrace(PTRACE_PEEKDATA, thread_id,
                                           (void*)(bp + sizeof(void*)), NULL);
            if (errno != 0) {
              HW_LOG("Child: PTRACE_PEEKDATA failed for return address at "
                     << std::hex << (bp + sizeof(void*)) << ": "
                     << strerror(errno));
              break;
            }
            if (return_addr == 0) {
              break;
            }
            stack_trace_stream << "  0x" << std::hex << return_addr << "\\n";

            errno = 0;
            uintptr_t next_bp =
                ptrace(PTRACE_PEEKDATA, thread_id, (void*)bp, NULL);
            if (errno != 0) {
              HW_LOG("Child: PTRACE_PEEKDATA failed for next fp at "
                     << std::hex << bp << ": " << strerror(errno));
              break;
            }
            if (next_bp <= bp) {
              HW_LOG("Child: Stack walk terminated to prevent loop: next_bp "
                     << std::hex << next_bp << " <= bp " << bp);
              break;
            }
            bp = next_bp;
          }

          HW_LOG("Child: Stack trace captured:\\n" << stack_trace_stream.str());

          // Write stack trace to pipe
          std::string stack_trace_str = stack_trace_stream.str();
          if (write(pipefd[1], stack_trace_str.c_str(),
                    stack_trace_str.length()) == -1) {
            HW_LOG("Child: Failed to write stack trace to pipe: "
                   << strerror(errno));
          } else {
            HW_LOG("Child: Stack trace written to pipe.");
          }
        } else {
          HW_LOG("Child: PTRACE_GETREGS failed for TID " << thread_id << ": "
                                                         << strerror(errno));
        }
#else
        HW_LOG("Child: Unsupported architecture for register dump");
        const char* placeholder = "Unsupported architecture";
        write(pipefd[1], placeholder, strlen(placeholder));

#endif
      } else {
        HW_LOG("Child: Thread "
               << thread_id << " not stopped as expected, status: " << status);
      }
      ptrace(PTRACE_DETACH, thread_id, NULL, NULL);
      close(pipefd[1]);  // Close write end
      HW_LOG("Child: Exiting");
      _exit(0);
    } else {
      // Parent process
      HW_LOG("Parent: process, child PID: " << child_pid);
      close(pipefd[1]);  // Close unused write end
      int child_status;
      waitpid(child_pid, &child_status, 0);
      HW_LOG("Parent: Child " << child_pid << " exited with status "
                              << child_status);

      // Read stack trace from pipe
      std::string stack_trace;
      char buffer[4096];
      ssize_t bytes_read;
      while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        stack_trace += buffer;
      }
      close(pipefd[0]);  // Close read end
      HW_LOG("Parent: Read stack trace from pipe:\n" << stack_trace);

      JNIEnv* env = base::android::AttachCurrentThread();
      if (!env) {
        HW_LOG("Parent: Failed to get JNIEnv.");
        return;
      }

      ScopedJavaLocalRef<jclass> util_class = base::android::GetClass(
          env, "package"
          "class");

      if (!util_class) {
        HW_LOG("Parent: Failed to find class");
        return;
      }

      jmethodID method_id =
          env->GetStaticMethodID(util_class.obj(), "reportHangFromNative",
                                 "(Ljava/lang/String;JJLjava/lang/String;)V");
      if (!method_id) {
        HW_LOG(
            "Parent: Failed to find method with (String, long, long, String) "
            "signature");
        return;
      }

      ScopedJavaLocalRef<jstring> j_threadTypeName =
          base::android::ConvertUTF8ToJavaString(env, thread_type_name);
      jlong j_threadId = static_cast<jlong>(thread_id);
      jlong j_hangDurationMs = static_cast<jlong>(hang_duration_ms);
      ScopedJavaLocalRef<jstring> j_stackTrace =
          base::android::ConvertUTF8ToJavaString(env, stack_trace);

      HW_LOG("Parent: Calling static method (4 args).");
      env->CallStaticVoidMethod(util_class.obj(), method_id,
                                j_threadTypeName.obj(), j_threadId,
                                j_hangDurationMs, j_stackTrace.obj());

      if (env->ExceptionCheck()) {
        HW_LOG("Parent: Exception occurred during JNI call.");
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
    }
  }
};

}  // namespace

void InstallAndroidHangReportHandler() {
  HW_LOG("InstallAndroidHangReportHandler");
  base::HangWatcher::SetHandler(std::make_unique<AndroidHangReportHandler>());
}

}  // namespace android
}  // namespace base
