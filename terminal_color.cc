#include "terminal_color.h"

TerminalColor::TerminalColor(std::unique_ptr<::Theme> theme)
    : theme_(std::move(theme)) {
  for (int i = 0; i < COLORS; i++) {
    free_colors_.push_back(i);
  }
  for (int i = 0; i < COLOR_PAIRS; i++) {
    free_pairs_.push_back(i);
  }
}

int TerminalColor::ColorToIndex(Theme::Color c) {
  RGB rgb(c.r, c.g, c.b);
  auto it = color_cache_.find(rgb);
  if (it != color_cache_.end()) {
    return it->second;
  }
  int n = free_colors_.back();
  free_colors_.pop_back();
  init_color(n, c.r * 1000 / 255, c.g * 1000 / 255, c.b * 1000 / 255);
  color_cache_.insert(std::make_pair(rgb, n));
  return n;
}

chtype TerminalColor::Theme(Token token, uint32_t flags) {
  auto key = std::make_pair(token, flags);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;

  Theme::Result r = theme_->ThemeToken(token, flags);

  int fg = ColorToIndex(r.foreground);
  int bg = ColorToIndex(r.background);
  auto pit = pair_cache_.find(std::make_pair(fg, bg));
  if (pit == pair_cache_.end()) {
    int n = free_pairs_.back();
    free_pairs_.pop_back();
    init_pair(n, fg, bg);
    pit = pair_cache_.insert(std::make_pair(std::make_pair(fg, bg), n)).first;
  }
  cache_.insert(std::make_pair(key, pit->second));
  return pit->second;
}
