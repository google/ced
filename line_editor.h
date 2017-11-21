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

#include <string>
#include <vector>
#include "selector.h"

struct AppEnv;

class LineEditor {
 public:
  void SelectLeft() {
    SelectMode(true);
    CursorLeft();
  }
  void MoveLeft() {
    SelectMode(false);
    CursorLeft();
  }
  void SelectRight() {
    SelectMode(true);
    CursorRight();
  }
  void MoveRight() { CursorRight(); }
  void MoveStartOfLine() { cursor_ = 0; }
  void MoveEndOfLine() { cursor_ = text_.length() - 1; }
  void MoveDownN(int n) {}
  void MoveUpN(int n) {}
  void MoveDown() {}
  void MoveUp() {}
  void SelectDownN(int n) {}
  void SelectUpN(int n) {}
  void SelectDown() {}
  void SelectUp() {}
  void Backspace() {
    if (cursor_ == 0) return;
    text_.erase(cursor_, 1);
    cursor_--;
  }
  void Copy(AppEnv* env) {}
  void Cut(AppEnv* env) {}
  void Paste(AppEnv* env) {}
  void InsChar(char c) {
    text_.insert(cursor_, 1, c);
    cursor_++;
  }

  template <class RC>
  void Render(RC* rc) {
    rc->Put(0, 0, text_, rc->color({}, 0));
  }

 private:
  void SelectMode(bool on) {
    if (on && sel_anchor_ == -1) {
      sel_anchor_ = cursor_;
    } else if (!on) {
      sel_anchor_ = -1;
    }
  }

  void CursorLeft() { cursor_ -= (cursor_ > 0); }
  void CursorRight() { cursor_ += (cursor_ < text_.length()); }

  int cursor_ = 0;
  int sel_anchor_ = -1;
  std::string text_;
};
