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
#include "theme.h"
#include <gtest/gtest.h>

TEST(Theme, DefaultParses) { Theme theme(Theme::DEFAULT); }

TEST(Theme, DefaultNoScope) {
  Theme theme(Theme::DEFAULT);
  Theme::Result r = theme.ThemeToken(Tag(), 0);
  EXPECT_EQ((Theme::Color{0x6c, 0x70, 0x79, 0xff}), r.foreground);
  EXPECT_EQ((Theme::Color{0x28, 0x2c, 0x34, 0xff}), r.background);
  EXPECT_EQ(Theme::Highlight::NONE, r.highlight);
  r = theme.ThemeToken(Tag(), 0);
  EXPECT_EQ((Theme::Color{0x6c, 0x70, 0x79, 0xff}), r.foreground);
  EXPECT_EQ((Theme::Color{0x28, 0x2c, 0x34, 0xff}), r.background);
  EXPECT_EQ(Theme::Highlight::NONE, r.highlight);
}
