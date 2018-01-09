#pragma once

#include <curses.h>
#include <tuple>
#include "theme.h"

class TerminalColor {
 public:
  explicit TerminalColor(std::unique_ptr<::Theme> theme);

  chtype Theme(::Theme::Tag token, uint32_t flags);
  chtype Lookup(CharFmt fmt);

  ::Theme* theme() { return theme_.get(); }

 private:
  typedef std::tuple<uint8_t, uint8_t, uint8_t> RGB;
  typedef std::tuple<float, float, float> LAB;

  static LAB RGB2LAB(RGB x);
  static float RGBDistance(RGB a, RGB b);

  int ColorToIndex(Color c);

  std::unique_ptr<::Theme> theme_;
  std::map<std::pair<::Theme::Tag, uint32_t>, chtype> cache_;
  std::map<CharFmt, chtype> cfcache_;
  std::map<RGB, int> color_cache_;
  std::map<std::pair<int, int>, chtype> pair_cache_;
  int next_color_ = 16;
  int next_pair_ = 1;
};
