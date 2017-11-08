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
#include "asm_parser.h"
#include <gtest/gtest.h>

TEST(AsmParser, Simple) {
  static const char* kInput =
      R"(/usr/local/google/tmp/ced.5wNoII.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <test()>:
_Z4testv():
/usr/local/google/home/ctiller/ced/<stdin>:5
   0: ret
)";

  static const char* kOutput = R"(test():
  ret
)";

  auto parsed = AsmParse(kInput);
  EXPECT_EQ(kOutput, parsed.body);
  EXPECT_EQ(1, parsed.src_to_asm_line[5].size());
  EXPECT_EQ(1, parsed.src_to_asm_line[5][0]);
}
