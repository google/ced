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
#include "plist.h"
#include <gtest/gtest.h>

TEST(PList, Empty) {
  auto src =
      R"(<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict/>
</plist>
  )";
  plist::NodePtr n = plist::Parse(src);
  EXPECT_NE(nullptr, n);
  EXPECT_NE(nullptr, n->AsDict());
}

TEST(PList, ArrayOfStrings) {
  auto src =
      R"(<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <string>foo</string>
  <string>bar</string>
</array>
</plist>
  )";
  plist::NodePtr n = plist::Parse(src);
  EXPECT_NE(nullptr, n);
  std::vector<std::string> expect = {"foo", "bar"};
  auto expect_it = expect.begin();
  for (const auto& c : *n->AsArray()) {
    EXPECT_EQ(*expect_it, *c->AsString());
    ++expect_it;
  }
}
