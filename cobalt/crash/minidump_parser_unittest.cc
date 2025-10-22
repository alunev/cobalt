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
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace android {

TEST(MinidumpParserTest, ParseValidMinidump) {
  // This expects 'latest.dmp' to be present in the test run directory.
  // You might need to adjust the path depending on how you run the test.
  base::FilePath dump_path = base::FilePath::FromUTF8Unsafe(
      "cobalt/crash/testdata/minidump_parser_test.dmp");

  std::vector<std::string> stacks = ParseMinidump(dump_path.value());

  // Based on our manual test, we expect it to find threads but maybe fail to
  // find the key OR if we provide a dump WITH the key (which we saw in
  // 'strings'), it should work.

  // If the key is missing (as we suspected), it returns empty.
  // If the key is found, it returns stacks.

  // For now, let's just assert we don't crash and print what we find.
  LOG(ERROR) << "Found " << stacks.size() << " stacks.";
  for (const auto& stack : stacks) {
    LOG(ERROR) << "Stack:\n" << stack;
  }
  ASSERT_FALSE(stacks.empty());
}

}  // namespace android
}  // namespace cobalt
