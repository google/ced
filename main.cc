// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gflags/gflags.h>
#include <grpc/support/log.h>
#include "application.h"
#include "log.h"

DEFINE_string(mode, DEFAULT_MODE,
              "Application mode (default " DEFAULT_MODE ")");

int main(int argc, char** argv) {
  gflags::SetUsageMessage("ced <filename.{h,cc}>");
  gflags::SetVersionString("0.0.0");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);
  gpr_set_log_function([](gpr_log_func_args* args) {
    Log() << args->file << ":" << args->line << ": " << args->message;
  });

#ifndef __APPLE__
  if (FLAGS_mode == "GUI" && nullptr == getenv("DISPLAY")) {
    FLAGS_mode = "Curses";
  }
#endif

  for (;;) {
    try {
      return Application::RunMode(FLAGS_mode, argc, argv);
    } catch (std::exception& e) {
      Log() << "FATAL EXCEPTION: " << e.what();
      return 1;
    } catch (...) {
      Log() << "FATAL EXCEPTION";
      return 1;
    }
  }
}
