// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/hang_report_handler_android.h"

#include <jni.h>
#include <memory>
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

  void OnHangDetected(const std::string& thread_type_name, PlatformThreadId thread_id, int64_t hang_duration_ms) override {
    HW_LOG("AndroidHangReportHandler::OnHangDetected - ENTRY");

    JNIEnv* env = base::android::AttachCurrentThread();
    if (!env) {
      HW_LOG("AndroidHangReportHandler::OnHangDetected: Failed to get JNIEnv.");
      return;
    }

    ScopedJavaLocalRef<jclass> util_class = base::android::GetClass(
        env, "com/google/android/libraries/youtube/systemhealth/hangdetection/"
        "NativeHangDetectorUtil");
    if (!util_class) {
      HW_LOG(
          "AndroidHangReportHandler::OnHangDetected: Failed to find class "
          "NativeHangDetectorUtil");
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
      return;
    }

    jmethodID method_id = env->GetStaticMethodID(
        util_class.obj(), "reportHangFromNative", "(Ljava/lang/String;JJ)V");
    if (!method_id) {
      HW_LOG(
          "AndroidHangReportHandler::OnHangDetected: Failed to find method "
          "reportHangFromNative with (String, long, long) signature");
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
      return;
    }

    ScopedJavaLocalRef<jstring> j_threadTypeName = base::android::ConvertUTF8ToJavaString(env, thread_type_name);
    jlong j_threadId = static_cast<jlong>(thread_id);
    jlong j_hangDurationMs = static_cast<jlong>(hang_duration_ms);

    HW_LOG("AndroidHangReportHandler::OnHangDetected: Calling static method with (String, long, long).");
    env->CallStaticVoidMethod(util_class.obj(), method_id, j_threadTypeName.obj(), j_threadId, j_hangDurationMs);

    if (env->ExceptionCheck()) {
      HW_LOG("AndroidHangReportHandler::OnHangDetected: Exception occurred.");
      env->ExceptionDescribe();
      env->ExceptionClear();
    }

    HW_LOG("AndroidHangReportHandler::OnHangDetected: Finished JNI call.");
  }
};

}  // namespace

void InstallAndroidHangReportHandler() {
  HW_LOG("InstallAndroidHangReportHandler");
  base::HangWatcher::SetHandler(
      std::make_unique<AndroidHangReportHandler>());
}

}  // namespace android
}  // namespace base