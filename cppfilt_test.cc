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
#include "cppfilt.h"
#include <gtest/gtest.h>
#include <thread>

#ifdef __APPLE__
#define MANGLED "__Z4testv"
#else
#define MANGLED "_Z4testv"
#endif

TEST(CppFilt, Simple) { EXPECT_EQ(cppfilt(MANGLED), "test()"); }

TEST(CppFilt, Threaded) {
  std::vector<std::thread> threads;
  for (int i = 0; i < 1000; i++) {
    threads.emplace_back([]() { EXPECT_EQ(cppfilt(MANGLED), "test()"); });
  }
  for (auto& t : threads) {
    t.join();
  }
}
