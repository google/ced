#pragma once

#include <curses.h>
#include <tuple>
#include "theme.h"

class TerminalColor {
 public:
  explicit TerminalColor(std::unique_ptr<::Theme> theme);

  chtype Theme(Token token, uint32_t flags);

 private:
  typedef std::tuple<uint8_t, uint8_t, uint8_t> RGB;

  int ColorToIndex(Theme::Color c);

  std::unique_ptr<::Theme> theme_;
  std::map<std::pair<Token, uint32_t>, chtype> cache_;
  std::map<RGB, int> color_cache_;
  std::map<std::pair<int, int>, chtype> pair_cache_;
  std::vector<int> free_colors_;
  std::vector<int> free_pairs_;
};
