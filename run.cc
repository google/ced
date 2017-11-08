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
#include "run.h"
#include <thread>
#include "include/subprocess.hpp"

std::string run(const std::string& command,
                const std::vector<std::string>& args,
                const std::string& input) {
  auto p = subprocess::popen(command, args);
  std::thread t([&]() {
    p.stdin() << input;
    p.close();
  });
  std::string result(std::istreambuf_iterator<char>(p.stdout()),
                     std::istreambuf_iterator<char>());
  t.join();
  return result;
}
