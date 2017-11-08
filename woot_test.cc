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
#include "woot.h"
#include "gtest/gtest.h"

TEST(String, NoOp) { String s; }

TEST(String, MutateThenRender) {
  String s;
  Site site;
  auto r = s.Insert(&site, 'a', String::Begin());
  EXPECT_EQ(s.Render(), "");
  EXPECT_EQ(r.str.Render(), "a");
  s = r.str;
  r = s.Insert(&site, 'b', r.command->id());
  EXPECT_EQ(r.str.Render(), "ab");
  s = r.str;
  auto rr = s.Remove(r.command->id());
  EXPECT_EQ(rr.str.Render(), "a");
}
