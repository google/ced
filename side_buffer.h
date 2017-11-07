#pragma once

#include <stddef.h>
#include <vector>
#include "token_type.h"

struct SideBuffer {
  std::vector<char> content;
  std::vector<Token> tokens;
  std::vector<size_t> line_ofs;

  void CalcLines();

  bool operator<(const SideBuffer& other) const {
    return content < other.content;
  }
};
