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

#ifndef COBALT_H5VCC_H5VCC_STACK_SPY_H_
#define COBALT_H5VCC_H5VCC_STACK_SPY_H_

#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/h5vcc/h5vcc_event_listener_container.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccStackSpy : public script::Wrappable {
 public:
  // Type for JavaScript event callback.
  typedef script::CallbackFunction<void(const std::string&)>
      H5vccStackTraceCallback;
  typedef script::ScriptValue<H5vccStackTraceCallback>
      H5vccStackTraceCallbackHolder;

  H5vccStackSpy(cobalt::browser::WebModule* web_module,
                script::EnvironmentSettings* environment_settings);

  void StackTrace(const H5vccStackTraceCallbackHolder& callback);

  void StartSpy(const unsigned int sampleIntervalMS);

  void StopSpy();

  void OnStackTrace(const H5vccStackTraceCallbackHolder& callback_holder);

  void RequestStackTrace();

  DEFINE_WRAPPABLE_TYPE(H5vccStackSpy);

 private:
  typedef script::ScriptValue<H5vccStackTraceCallback>::Reference
      H5vccStackTraceCallbackReference;

  void StackTraceCaptured(const std::string& stack_trace);

  // Scoped holder of the listeners that will be notified when a response
  // occurs.
  H5vccEventListenerContainer<const std::string&, H5vccStackTraceCallback>
      listeners_;

  browser::WebModule* web_module_;

  script::EnvironmentSettings* environment_settings_;

  // Task timer that requests a stack trace at an interval.
  base::RepeatingTimer timer_;

  // Thread checker for the thread that creates this instance.
  THREAD_CHECKER(thread_checker_);

  // Track the message loop that created this object so stack trace events are
  // handled from the same thread.
  base::MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(H5vccStackSpy);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_STACK_SPY_H_
