#include "terminal_color.h"
#include "log.h"

TerminalColor::TerminalColor(std::unique_ptr<::Theme> theme)
    : theme_(std::move(theme)) {
  Log() << "CAN_CHANGE_COLOR: " << can_change_color();
  Log() << "HAS_COLORS: " << has_colors();
}

int TerminalColor::ColorToIndex(Theme::Color c) {
  RGB rgb(c.r, c.g, c.b);
  auto it = color_cache_.find(rgb);
  if (it != color_cache_.end()) {
    return it->second;
  }
  int n = next_color_++;
  short r, g, b;
  color_content(n, &r, &g, &b);
  Log() << "crrent to " << r << "," << g << "," << b;
  Log() << "COLOR[" << n << "]: " << (int)(c.r * 1000 / 255) << " "
        << (int)(c.g * 1000 / 255) << " " << (int)(c.b * 1000 / 255);
  Log() << init_color(n, c.r * 1000 / 255, c.g * 1000 / 255, c.b * 1000 / 255);
  color_content(n, &r, &g, &b);
  Log() << "set to " << r << "," << g << "," << b;
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
    int n = next_pair_++;
    Log() << "PAIR[" << n << "]: " << fg << "," << bg;
    Log() << init_pair(n, fg, bg);
    pit = pair_cache_.insert(std::make_pair(std::make_pair(fg, bg), n)).first;
  }
  return cache_.insert(std::make_pair(key, COLOR_PAIR(pit->second)))
      .first->second;
}
