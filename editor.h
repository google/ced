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

struct AppEnv {
  std::string clipboard;
};

class Editor {
 public:
  Editor(Site* site) : site_(site), ed_(site) {}

  // state management
  void UpdateState(LogTimer* tmr, const EditNotification& state);
  const EditNotification& CurrentState() { return state_; }
  bool HasCommands() {
    return state_.shutdown || !unpublished_commands_.commands().empty() ||
           cursor_reported_ != cursor_;
  }
  EditResponse MakeResponse();

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
  void Copy(AppEnv* env);
  void Cut(AppEnv* env);
  void Paste(AppEnv* env);
  void InsNewLine() { InsChar('\n'); }
  void InsChar(char c);

  template <class RC>
  std::function<void(RC* ctx)> PrepareRender() {
    auto content = state_.content;
    return [this, content](RC* ctx) {
      if (cursor_row_ < 0) {
        cursor_row_ = 0;
      } else if (cursor_row_ >= ctx->window->height()) {
        cursor_row_ = ctx->window->height() - 1;
      }

      cursor_ = AnnotatedString::Iterator(content, cursor_).id();
      AnnotatedString::LineIterator line_cr(content, cursor_);
      AnnotatedString::LineIterator line_bk = line_cr;
      AnnotatedString::LineIterator line_fw = line_cr;
      for (int i = 0; i < ctx->window->height(); i++) {
        line_bk.MovePrev();
        line_fw.MoveNext();
      }
      AnnotatedString::AllIterator it = line_bk.AsAllIterator();
      int nrow = 0;
      int ncol = 0;
      uint32_t base_flags = 0;
      AnnotatedString::AllIterator start_of_line = it;
      std::vector<std::string> gutter_annotations;
      auto add_gutter = [&gutter_annotations](const std::string& s) {
        for (const auto& g : gutter_annotations) {
          if (g == s) return;
        }
        gutter_annotations.push_back(s);
      };
      while (it.id() != line_fw.id()) {
        if (it.is_visible()) {
          uint32_t chr_flags = base_flags;
          std::vector<std::string> tags;
          bool move_cursor = false;
          bool has_diagnostic = false;
          it.ForEachAttrValue([&](const Attribute& attr) {
            switch (attr.data_case()) {
              case Attribute::kCursor:
                Log() << "found cursor " << nrow << " " << ncol;
                move_cursor = true;
                break;
              case Attribute::kSelection:
                chr_flags |= Theme::SELECTED;
                break;
              case Attribute::kDiagnostic:
                has_diagnostic = true;
                break;
              case Attribute::kTags:
                for (auto t : attr.tags().tags()) {
                  tags.push_back(t);
                }
                break;
              case Attribute::kSize:
                switch (attr.size().type()) {
                  case SizeAnnotation::OFFSET_INTO_PARENT:
                    if (attr.size().bits()) {
                      add_gutter(absl::StrCat("@", attr.size().size(), ".",
                                              attr.size().bits()));
                    } else {
                      add_gutter(absl::StrCat("@", attr.size().size()));
                    }
                    break;
                  case SizeAnnotation::SIZEOF_SELF:
                    if (attr.size().bits()) {
                      add_gutter(absl::StrCat(
                          attr.size().size() * 8 + attr.size().bits(), "b"));
                    } else {
                      add_gutter(absl::StrCat(attr.size().size(), "B"));
                    }
                    break;
                  default:
                    break;
                }
                break;
              default:
                break;
            }
          });
          if (has_diagnostic) {
            tags.push_back("invalid");
          }
          if (it.value() == '\n') {
            auto fill_attr = ctx->color->Theme(::Theme::Tag(), base_flags);
            while (ncol < ctx->window->width()) {
              ctx->Put(nrow, ncol, ' ', fill_attr);
              ncol++;
            }
            std::string gutter = absl::StrJoin(gutter_annotations, ",");
            ncol = ctx->window->width() - gutter.length();
            fill_attr =
                ctx->color->Theme(::Theme::Tag{"comment.gutter"}, base_flags);
            for (auto c : gutter) {
              ctx->Put(nrow, ncol, c, fill_attr);
              ncol++;
            }
            gutter_annotations.clear();
            nrow++;
            ncol = 0;
            start_of_line = it;
            base_flags = 0;
          } else {
            ctx->Put(nrow, ncol, it.value(),
                     ctx->color->Theme(tags, chr_flags));
            ncol++;
          }
          if (move_cursor) {
            ctx->Move(nrow, ncol);
            if ((base_flags & Theme::HIGHLIGHT_LINE) == 0) {
              ncol = 0;
              it = start_of_line;
              base_flags |= Theme::HIGHLIGHT_LINE;
            }
          }
        }
        it.MoveNext();
      }
    };
  }

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

  Site* const site_;
  int cursor_row_ = 0;  // cursor row as an offset into the view buffer
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
};
