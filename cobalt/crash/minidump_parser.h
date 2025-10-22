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

#ifndef COBALT_CRASH_MINIDUMP_PARSER_H_
#define COBALT_CRASH_MINIDUMP_PARSER_H_

#include <string>
#include <vector>

namespace cobalt {
namespace android {

// Parses the minidump at |minidump_path|.
// It looks for the "list-of-hung-threads" crash key to identify which threads
// caused the hang.
// Returns a vector of stack traces (one for each hung thread found).
// If no hung threads are found (or parsing fails), returns an empty vector.
std::vector<std::string> ParseMinidump(const std::string& minidump_path);

}  // namespace android
}  // namespace cobalt

#endif  // COBALT_CRASH_MINIDUMP_PARSER_H_
