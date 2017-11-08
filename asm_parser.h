#pragma once

#include <string>

struct AsmParseResult {
  std::string body;
};

AsmParseResult AsmParse(const std::string& src);

