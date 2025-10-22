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

#include "cobalt/app/cobalt_crash_reporter_client.h"

#include <string>

#include "base/check.h"
#include "base/path_service.h"

void CobaltCrashReporterClient::Create() {
  static base::NoDestructor<CobaltCrashReporterClient> crash_client;
  crash_reporter::SetCrashReporterClient(crash_client.get());
}

CobaltCrashReporterClient::CobaltCrashReporterClient() {}

CobaltCrashReporterClient::~CobaltCrashReporterClient() {}

void CobaltCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  CHECK(product_name);
  CHECK(version);
  *product_name = "Chrobalt_ATV";
  // TODO: hwarriner - get the actual app version.
  *version = "1.23.23";
}

bool CobaltCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  crash_dump_location_ = new base::FilePath(
      "/data/data/com.google.android.youtube.tv/cache/crashpad");
  *crash_dir = *crash_dump_location_;
  return true;
}

bool CobaltCrashReporterClient::IsRunningUnattended() {
  // TODO: hwarriner - may need to somehow return true for automated tests.
  return false;
}
