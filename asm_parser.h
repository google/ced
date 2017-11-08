#pragma once

#include <map>
#include <string>
#include <vector>

struct AsmParseResult {
  std::string body;
  std::map<int, std::vector<int>> src_to_asm_line;
};

AsmParseResult AsmParse(const std::string& src);
