#include <gtest/gtest.h>
#include "asm_parser.h"

TEST(AsmParser, Simple) {
  static const char* kInput = R"(/usr/local/google/tmp/ced.5wNoII.o:     file format elf64-x86-64


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

