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
#include "fswatch.h"
#include <stdio.h>

int main(int argc, char** argv) {
  std::vector<std::string> s;
  for (int i = 1; i < argc; i++) {
    s.push_back(argv[i]);
  }
  std::unique_ptr<FSWatcher> w;
  std::function<void(bool)> cb = [&](bool shutdown) {
    if (!shutdown) {
      puts("file changed");
      w.reset(new FSWatcher(s, cb));
    } else {
      puts("shutdown");
    }
  };
  cb(false);
  getchar();
}
