// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION < 16

#include "starboard/directory.h"

#include <windows.h>
#include <vector>

#include "starboard/shared/win32/directory_internal.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::DirectoryExists;
using starboard::shared::win32::DirectoryExistsOrCreated;
using starboard::shared::win32::IsAbsolutePath;
using starboard::shared::win32::NormalizeWin32Path;
using starboard::shared::win32::TrimExtraFileSeparators;

bool SbDirectoryCreate(const char* path) {
  if ((path == nullptr) || (path[0] == '\0')) {
    return false;
  }

  std::wstring path_wstring = NormalizeWin32Path(path);
  TrimExtraFileSeparators(&path_wstring);

  if (!IsAbsolutePath(path_wstring)) {
    return false;
  }

  return DirectoryExistsOrCreated(path_wstring);
}
#endif  // SB_API_VERSION < 16
