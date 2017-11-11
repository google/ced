// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "token_type.h"

class Theme {
 public:
  explicit Theme(const std::string& file);
  enum default_type { DEFAULT };
  explicit Theme(default_type);

  enum class Highlight : uint8_t {
    UNSET,
    NONE,
    BOLD,
    UNDERLINE,
    ITALIC,
    STRIKE
  };

  struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    bool operator==(const Color& other) const {
      return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color& other) const { return !operator==(other); }
  };

  static constexpr uint32_t HIGHLIGHT_LINE = 1;

  struct Result {
    Color foreground;
    Color background;
    Highlight highlight;
  };
  Result ThemeToken(Token token, uint32_t flags);

 private:
  void Load(const std::string& src);

  typedef absl::optional<Color> OptColor;

  struct Setting {
    std::vector<Selector> scopes;
    Highlight font_style = Highlight::UNSET;
    Highlight bracket_contents_options = Highlight::UNSET;
    Highlight brackets_options = Highlight::UNSET;
    Highlight tags_options = Highlight::UNSET;
    int shadow_width = -1;
    OptColor foreground;
    OptColor background;
    OptColor invisibles;
    OptColor caret;
    OptColor line_highlight;
    OptColor bracket_contents_foreground;
    OptColor brackets_foreground;
    OptColor tags_foreground;
    OptColor find_highlight;
    OptColor find_highlight_foreground;
    OptColor gutter;
    OptColor gutter_foreground;
    OptColor selection;
    OptColor selection_border;
    OptColor inactive_selection;
    OptColor guide;
    OptColor active_guide;
    OptColor stack_guide;
    OptColor highlight;
    OptColor highlight_foreground;
    OptColor shadow;
  };

  template <OptColor(Setting::*Field)>
  struct LoadColor {
    void operator()(const std::string& value, Setting* setting) {
      if (value.empty()) {
        setting->*Field = OptColor();
      }
      int inc = 0;
      if (value[inc] == '#') inc++;
      char* end;
      long hex = strtol(value.c_str() + inc, &end, 16);
      if (*end) throw std::runtime_error("Failed to parse color");
      Color c;
      c.r = (hex >> 16) & 0xff;
      c.g = (hex >> 8) & 0xff;
      c.b = (hex)&0xff;
      if (value.length() <= 7) {
        c.a = 255;
      } else {
        c.a = (hex >> 24) & 0xff;
      }
      (setting->*Field) = c;
    }
  };
  template <Highlight(Setting::*Field)>
  struct LoadHighlight {
    void operator()(const std::string& value, Setting* setting) {
      if (value == "underline") {
        setting->*Field = Highlight::UNDERLINE;
      } else if (value == "italic") {
        setting->*Field = Highlight::ITALIC;
      } else if (value == "bold") {
        setting->*Field = Highlight::BOLD;
      } else if (value == "bold italic") {
        setting->*Field = Highlight::ITALIC;
      } else if (value == "strike") {
        setting->*Field = Highlight::STRIKE;
      } else if (value.empty() || value == "normal") {
        setting->*Field = Highlight::NONE;
      } else {
        throw std::runtime_error("Unknown highlight " + value);
      }
    }
  };
  template <int(Setting::*Field)>
  struct LoadInt {
    void operator()(const std::string& value, Setting* setting) {
      if (1 != sscanf(value.c_str(), "%d", &(setting->*Field))) {
        throw std::runtime_error("Not an integer");
      }
    }
  };
  static void LoadIgnored(const std::string& value, Setting* setting) {}

  std::vector<Setting> settings_;
  std::map<std::pair<Token, uint32_t>, Result> theme_cache_;
};
