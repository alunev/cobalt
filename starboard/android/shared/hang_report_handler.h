// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STARBOARD_ANDROID_SHARED_HANG_REPORT_HANDLER_H_
#define STARBOARD_ANDROID_SHARED_HANG_REPORT_HANDLER_H_

#include "base/threading/hang_watcher.h"

namespace starboard {
namespace android {

// Creates and installs the Android-specific HangReportHandler.
void InstallAndroidHangReportHandler();

}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_HANG_REPORT_HANDLER_H_
