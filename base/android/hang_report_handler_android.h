// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_HANG_REPORT_HANDLER_ANDROID_H_
#define BASE_ANDROID_HANG_REPORT_HANDLER_ANDROID_H_

#include "base/threading/hang_watcher.h"

namespace base {
namespace android {

// Creates and installs the Android-specific HangReportHandler.
void InstallAndroidHangReportHandler();

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_HANG_REPORT_HANDLER_ANDROID_H_