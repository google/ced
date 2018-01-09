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

#include <stdint.h>

enum class Highlight : uint8_t { UNSET, NONE, BOLD, UNDERLINE, ITALIC, STRIKE };

struct Color {
  Color(uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa) {
    r = rr;
    g = gg;
    b = bb;
    a = aa;
  }
  Color() = default;

  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      uint8_t a;
    };
    uint32_t rgba;
  };

  bool operator==(Color other) const { return rgba == other.rgba; }
  bool operator!=(Color other) const { return !operator==(other); }
  bool operator<(Color other) const { return rgba < other.rgba; }
  bool operator>(Color other) const { return rgba > other.rgba; }
};

struct CharFmt {
  Color foreground;
  Color background;
  Highlight highlight;

  bool operator<(const CharFmt& other) const {
    if (foreground < other.foreground) return true;
    if (foreground > other.foreground) return false;
    if (background < other.background) return true;
    if (background > other.background) return false;
    if (highlight < other.highlight) return true;
    if (highlight > other.highlight) return false;
    return false;
  }

  bool operator==(const CharFmt& other) const {
    return foreground == other.foreground && background == other.background &&
           highlight == other.highlight;
  }

  bool operator!=(const CharFmt& other) const { return !operator==(other); }
};
