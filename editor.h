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

#include <numeric>
#include <string>
#include "absl/strings/str_join.h"
#include "buffer.h"
#include "log.h"
#include "render.h"
#include "theme.h"

class Editor : public std::enable_shared_from_this<Editor> {
 public:
  static std::shared_ptr<Editor> Make(Site* site, const std::string& name,
                                      bool editable) {
    return std::shared_ptr<Editor>(new Editor(site, name, editable));
  }

 private:
  Editor(Site* site, const std::string& name, bool editable)
      : site_(site), name_(name), editable_(editable), ed_(site) {}

 public:
  // state management
  void UpdateState(LogTimer* tmr, const EditNotification& state);
  const EditNotification& CurrentState() { return state_; }
  bool HasCommands() {
    return state_.shutdown || !unpublished_commands_.commands().empty() ||
           cursor_reported_ != cursor_;
  }
  EditResponse MakeResponse();

  std::vector<std::string> DebugData() const;

  // editor commands
  void SelectLeft();
  void MoveLeft();
  void SelectRight();
  void MoveRight();
  void MoveStartOfLine();
  void MoveEndOfLine();
  void MoveDownN(int n);
  void MoveUpN(int n);
  void MoveDown() { MoveDownN(1); }
  void MoveUp() { MoveUpN(1); }
  void SelectDownN(int n);
  void SelectUpN(int n);
  void SelectDown() { SelectDownN(1); }
  void SelectUp() { SelectUpN(1); }
  void Backspace();
  void Copy(Renderer* env);
  void Cut(Renderer* env);
  void Paste(Renderer* env);
  void InsNewLine() { InsChar('\n'); }
  void InsChar(char c);

  void Render(Theme* theme, Widget* parent);

 private:
  void CursorLeft();
  void CursorRight();
  void CursorDown();
  void CursorUp();
  void CursorStartOfLine();
  void CursorEndOfLine();
  void PublishCursor();

  void SetSelectMode(bool sel);
  bool SelectMode() const { return selection_anchor_ != ID(); }
  void DeleteSelection();

  void ChangeCursorLine(int delta) {
    if (delta) {
      cursor_line_.set_value(cursor_line_.value() + delta);
    }
  }
  static void RenderLine(DeviceContext* ctx, const Device::Extents& extents,
                         Theme* theme, ID cursor, int y,
                         AnnotatedString::LineIterator lit, bool highlight);

  Site* const site_;
  const std::string name_;
  const bool editable_;
  rhea::variable cursor_line_{};
  ID cursor_ = AnnotatedString::Begin();
  ID cursor_reported_ = AnnotatedString::End();
  ID selection_anchor_ = ID();
  EditNotification state_;
  CommandSet unpublished_commands_;
  CommandSet unacknowledged_commands_;
  AnnotationEditor ed_;
  struct BufferInfo {
    std::unique_ptr<Buffer> buffer;
    AnnotationEditor ed;
  };
  std::map<ID, BufferInfo> buffers_;

  // debug values
  struct {
    int nrow_before_sub = 0;
    int window_height = 0;
  } debug_;
};
