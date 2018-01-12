#include "terminal_color.h"
#include <math.h>
#include "log.h"

TerminalColor::TerminalColor(std::unique_ptr<::Theme> theme)
    : theme_(std::move(theme)) {
  Log() << "CAN_CHANGE_COLOR: " << can_change_color();
  Log() << "HAS_COLORS: " << has_colors();
}

TerminalColor::LAB TerminalColor::RGB2LAB(RGB rgb) {
  float r = static_cast<float>(std::get<0>(rgb)) / 255.0f;
  float g = static_cast<float>(std::get<1>(rgb)) / 255.0f;
  float b = static_cast<float>(std::get<2>(rgb)) / 255.0f;
  float x, y, z;

  r = (r > 0.04045f) ? powf((r + 0.055f) / 1.055f, 2.4f) : r / 12.92f;
  g = (g > 0.04045f) ? powf((g + 0.055f) / 1.055f, 2.4f) : g / 12.92f;
  b = (b > 0.04045f) ? powf((b + 0.055f) / 1.055f, 2.4f) : b / 12.92f;

  x = (r * 0.4124f + g * 0.3576f + b * 0.1805f) / 0.95047f;
  y = (r * 0.2126f + g * 0.7152f + b * 0.0722f) / 1.00000f;
  z = (r * 0.0193f + g * 0.1192f + b * 0.9505f) / 1.08883f;

  x = (x > 0.008856f) ? powf(x, 1.0f / 3.0f) : (7.787f * x) + 16.0f / 116.0f;
  y = (y > 0.008856f) ? powf(y, 1.0f / 3.0f) : (7.787f * y) + 16.0f / 116.0f;
  z = (z > 0.008856f) ? powf(z, 1.0f / 3.0f) : (7.787f * z) + 16.0f / 116.0f;

  return LAB((116.0f * y) - 16.0f, 500.0f * (x - y), 200.0f * (y - z));
}

template <int I, class T>
static T sqdiff(std::tuple<T, T, T> a, std::tuple<T, T, T> b) {
  T d = std::get<I>(a) - std::get<I>(b);
  return d * d;
}

float TerminalColor::RGBDistance(RGB a, RGB b) {
  LAB la = RGB2LAB(a);
  LAB lb = RGB2LAB(b);
  return sqdiff<0>(la, lb) + sqdiff<1>(la, lb) + sqdiff<2>(la, lb);
}

int TerminalColor::ColorToIndex(Color c) {
  RGB rgb(c.r, c.g, c.b);
  auto it = color_cache_.find(rgb);
  if (it != color_cache_.end()) {
    return it->second;
  }

  if (can_change_color()) {
    int n = next_color_++;
    short r, g, b;
    color_content(n, &r, &g, &b);
    Log() << "crrent to " << r << "," << g << "," << b;
    Log() << "COLOR[" << n << "]: " << (int)(c.r * 1000 / 255) << " "
          << (int)(c.g * 1000 / 255) << " " << (int)(c.b * 1000 / 255);
    Log() << init_color(n, c.r * 1000 / 255, c.g * 1000 / 255,
                        c.b * 1000 / 255);
    color_content(n, &r, &g, &b);
    Log() << "set to " << r << "," << g << "," << b;
    color_cache_.insert(std::make_pair(rgb, n));
    return n;
  } else {
    float best_diff = -1;
    int best_clr = -1;
    for (int i = 0; i < COLORS; i++) {
      short r, g, b;
      color_content(i, &r, &g, &b);
      RGB sys(r * 255 / 1000, g * 255 / 1000, b * 255 / 1000);
      float diff = RGBDistance(rgb, sys);
      Log() << "color[" << i << " is " << r << "," << g << "," << b
            << " ; diff=" << diff;
      if (i == 0 || best_diff > diff) {
        best_diff = diff;
        best_clr = i;
      }
    }
    color_cache_.insert(std::make_pair(rgb, best_clr));
    return best_clr;
  }
}

chtype TerminalColor::Theme(::Theme::Tag token, uint32_t flags) {
  auto key = std::make_pair(token, flags);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;

  chtype c = Lookup(theme_->ThemeToken(token, flags));
  cache_.insert(std::make_pair(key, c));
  return c;
}

chtype TerminalColor::Lookup(CharFmt fmt) {
  auto it = cfcache_.find(fmt);
  if (it != cfcache_.end()) return it->second;

  int fg = ColorToIndex(fmt.foreground);
  int bg = ColorToIndex(fmt.background);
  auto pit = pair_cache_.find(std::make_pair(fg, bg));
  if (pit == pair_cache_.end()) {
    int n = next_pair_++;
    Log() << "PAIR[" << n << "]: " << fg << "," << bg;
    Log() << init_pair(n, fg, bg);
    pit = pair_cache_.insert(std::make_pair(std::make_pair(fg, bg), n)).first;
  }
  chtype highlight_or = 0;
  switch (fmt.highlight) {
    case Highlight::UNSET:
    case Highlight::NONE:
      break;
    case Highlight::UNDERLINE:
      highlight_or = A_UNDERLINE;
      break;
    case Highlight::BOLD:
      highlight_or = A_BOLD;
      break;
    case Highlight::ITALIC:
#ifdef A_ITALIC
      highlight_or = A_ITALIC;
#endif
      break;
    case Highlight::STRIKE:
      highlight_or = A_BLINK;
      break;
  }
  return cfcache_
      .insert(std::make_pair(fmt, COLOR_PAIR(pit->second) | highlight_or))
      .first->second;
}
