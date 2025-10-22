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

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Generated JNI header.
#include "cobalt/android/jni_headers/MinidumpParser_jni.h"

namespace cobalt {
namespace android {

static base::android::ScopedJavaLocalRef<jobjectArray>
JNI_MinidumpParser_GetStackTracesFromMinidump(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_minidump_path) {
  std::string minidump_path =
      base::android::ConvertJavaStringToUTF8(env, j_minidump_path);
  std::vector<std::string> results = ParseMinidump(minidump_path);

  return base::android::ToJavaArrayOfStrings(env, results);
}

}  // namespace android
}  // namespace cobalt
