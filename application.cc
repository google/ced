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

#include "application.h"
#include <iostream>
#include <unordered_map>
#include "log.h"

namespace {

struct Registry {
  static Registry& Get() {
    static Registry r;
    return r;
  }

  std::unordered_map<std::string, std::function<std::unique_ptr<Application>(
                                      int argc, char** argv)>>
      modes;
};

}  // namespace

int Application::Register(
    const std::string& mode,
    std::function<std::unique_ptr<Application>(int argc, char** argv)>
        construct) {
  Registry::Get().modes[mode] = construct;
  return 0;
}

int Application::RunMode(const std::string& mode, int argc, char** argv) {
  const auto& m = Registry::Get().modes;
  if (mode == "list") {
    std::cout << "Available modes:\n";
    int n = 1;
    std::cout << (n++) << ". list\n";
    for (const auto& mi : m) {
      std::cout << (n++) << ". " << mi.first << "\n";
    }
    return 0;
  }
  auto it = m.find(mode);
  if (it == m.end()) {
    Log() << "Bad application mode: " << mode;
    return 1;
  }
  return it->second(argc, argv)->Run();
}
