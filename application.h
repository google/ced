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
#pragma once

#include <functional>
#include <memory>
#include <string>

class Application {
 public:
  virtual ~Application() {}
  static int Register(
      const std::string& mode,
      std::function<std::unique_ptr<Application>(int argc, char** argv)>
          construct);
  static int RunMode(const std::string& mode, int argc, char** argv);

  virtual int Run() = 0;
};

#define REGISTER_APPLICATION(cls)                                   \
  namespace {                                                       \
  const int r_##cls =                                               \
      ::Application::Register(#cls, [](int argc, char** argv) {     \
        return std::unique_ptr<::Application>(new cls(argc, argv)); \
      });                                                           \
  }
