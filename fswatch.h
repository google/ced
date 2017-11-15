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

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class FSWatcher {
 public:
  FSWatcher(const std::vector<std::string>& interest_set,
            std::function<void(bool shutting_down)> callback);
  ~FSWatcher();

  FSWatcher(const FSWatcher& other) = delete;
  FSWatcher& operator=(const FSWatcher& other) = delete;

 private:
  static constexpr int kWriteEnd = 1;
  static constexpr int kReadEnd = 0;
  int pipe_[2];
  std::thread watch_;
};
