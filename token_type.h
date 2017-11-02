#pragma once

#include <stdint.h>

enum class Token : uint8_t { UNSET, IDENT, KEYWORD, SYMBOL, LITERAL, COMMENT };
