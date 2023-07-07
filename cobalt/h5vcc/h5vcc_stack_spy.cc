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

#include "cobalt/h5vcc/h5vcc_stack_spy.h"

namespace cobalt {
namespace h5vcc {

namespace {
const std::string& OnGetArgument(const std::string& stack_trace) {
  return stack_trace;
}
}  // namespace

H5vccStackSpy::H5vccStackSpy(cobalt::browser::WebModule* web_module,
                             script::EnvironmentSettings* environment_settings)
    : ALLOW_THIS_IN_INITIALIZER_LIST(listeners_(this)),
      web_module_(web_module),
      environment_settings_(environment_settings),
      message_loop_(base::MessageLoop::current()) {}

void H5vccStackSpy::StartSpy(const unsigned int sampleIntervalMS) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&H5vccStackSpy::StartSpy, base::Unretained(this),
                              sampleIntervalMS));
    return;
  }
  if (!timer_.IsRunning()) {
    timer_.SetTaskRunner(message_loop_->task_runner());
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(sampleIntervalMS),
                 base::BindRepeating(&H5vccStackSpy::RequestStackTrace,
                                     base::Unretained(this)));
  }
}

void H5vccStackSpy::StopSpy() {
  timer_.Stop();
}

void H5vccStackSpy::RequestStackTrace() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&H5vccStackSpy::RequestStackTrace, base::Unretained(this)));
    return;
  }

  web_module_->RequestJavaScriptStackTrace(
      base::Bind(&H5vccStackSpy::StackTraceCaptured, base::Unretained(this)));
}

void H5vccStackSpy::StackTraceCaptured(const std::string& stack_trace) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&H5vccStackSpy::StackTraceCaptured,
                              base::Unretained(this), stack_trace));
    return;
  }

  listeners_.DispatchEvent(base::Bind(OnGetArgument, stack_trace));
}

void H5vccStackSpy::OnStackTrace(
    const H5vccStackTraceCallbackHolder& callback_holder) {
  listeners_.AddListener(callback_holder);
}

}  // namespace h5vcc
}  // namespace cobalt