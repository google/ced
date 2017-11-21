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
  void UpdateState(const EditNotification& state);
  const EditNotification& CurrentState() { return state_; }
  bool HasCommands() {
    return state_.shutdown || !unpublished_commands_.commands().empty();
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
    return [this](RC* ctx) {
      if (cursor_row_ < 0) {
        cursor_row_ = 0;
      } else if (cursor_row_ >= window_height) {
        cursor_row_ = window_height - 1;
      }

      cursor_ = AnnotatedString::Iterator(state_.content, cursor_).id();
      AnnotatedString::LineIterator line_cr(state_.content, cursor_);
      AnnotatedString::LineIterator line_end_cr = line_cr.Next();
      AnnotatedString::LineIterator line_bk = line_cr;
      AnnotatedString::LineIterator line_fw = line_cr;
      for (int i = 0; i < window_height; i++) {
        line_bk.MovePrev();
        line_fw.MoveNext();
      }
      AnnotatedString::AllIterator it = line_bk.AsAllIterator();
      AnnotationTracker<Tag> t_token(state_.token_types);
      AnnotationTracker<ID> t_diagnostic(state_.diagnostic_ranges);
      AnnotationTracker<SideBufferRef> t_side_buffer_ref(
          state_.side_buffer_refs);
      std::vector<CharInfo> ci;
      int nrow = 0;
      int ncol = 0;
      uint32_t base_flags = 0;
      AnnotatedString::AllIterator start_of_line = it;
      std::vector<std::string> gutter_annotations;
      while (it.id() != line_fw.id()) {
        if (it.is_visible()) {
          if (it.value() == '\n') {
            LineInfo li{it.id() == line_end_cr.id(),
                        absl::StrJoin(gutter_annotations, ",")};
            gutter_annotations.clear();
            nrow++;
            ncol = 0;
            start_of_line = it.Next();
          } else {
            uint32_t line_flags = base_flags;
            ctx->Put(nrow, ncol, it.value(),
                     ctx->color->Theme(it, &line_flags));
            if (line_flags == base_flags) {
              ncol++;
            } else {
              base_flags = line_flags;
              ncol = 0;
              it = start_of_line;
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

  void SetSelectMode(bool sel);
  bool SelectMode() const { return selection_anchor_ != ID(); }
  void DeleteSelection();

#if 0
  template <class RC>
  struct EditRenderContext {
    RC* parent_context;
    decltype(static_cast<RC*>(nullptr)->color) color;
    const typename Renderer<EditRenderContext>::Rect* window;
    int ofs_row;

    template <class T, class A>
    void Put(int row, int col, const T& val, const A& attr) {
      if (row < 0 || col < 0 || row >= window->height() ||
          col >= window->width()) {
        return;
      }
      parent_context->Put(row + ofs_row + window->row(), col + window->column(),
                          val, attr);
    }

    void Move(int row, int col) {
      Log() << "EDMOVE: " << row << ", " << col;
      if (row < 0 || col < 0 || row >= window->height() ||
          col >= window->width()) {
        return;
      }
      parent_context->Move(row + ofs_row + window->row(),
                           col + window->column());
    }
  };

  template <class RC>
  void RenderThing(int (Editor::*thing)(int height, LineCallback),
                   int (Editor::*compute_row_offset)(
                       int height, int first_drawn,
                       const std::vector<int>& rows, bool* animating),
                   RC* parent_context) {
    Renderer<EditRenderContext<RC>> renderer;
    auto container = renderer.AddContainer(LAY_COLUMN)
                         .FixSize(parent_context->window->width(), 0);
    std::vector<typename Renderer<EditRenderContext<RC>>::ItemRef> rows;
    int first_drawn = (this->*thing)(
        parent_context->window->height(),
        [&container, &rows, parent_context](LineInfo li,
                                            const std::vector<CharInfo>& ci) {
          auto line_container =
              container.AddContainer(LAY_TOP | LAY_LEFT, LAY_ROW)
                  .FixSize(parent_context->window->width(), 1);
          auto ref = line_container
                         .AddItem(LAY_TOP | LAY_LEFT,
                                  [li, ci](EditRenderContext<RC>* ctx) {
                                    int col = 0;
                                    for (const auto& c : ci) {
                                      if (c.cursor) {
                                        ctx->Move(0, col);
                                      }
                                      ctx->Put(0, col++, c.c,
                                               ctx->color->Theme(
                                                   c.tag, ThemeFlags(&li, &c)));
                                    }
                                  })
                         .FixSize(ci.size(), 1);
          line_container.AddItem(LAY_FILL, [li](EditRenderContext<RC>* ctx) {
            for (int i = 0; i < ctx->window->width(); i++) {
              ctx->Put(0, i, ' ',
                       ctx->color->Theme(Tag(), ThemeFlags(&li, nullptr)));
            }
          });
          if (!li.gutter_annotation.empty()) {
            line_container
                .AddItem(LAY_RIGHT,
                         [li](EditRenderContext<RC>* ctx) {
                           ctx->Put(
                               0, 0, li.gutter_annotation,
                               ctx->color->Theme(Tag().Push("comment.gutter"),
                                                 ThemeFlags(&li, nullptr)));
                         })
                .FixSize(li.gutter_annotation.length(), 1);
          }
          if (li.cursor) {
            rows.push_back(ref);
          }
        });
    renderer.Layout();
    std::vector<int> row_idx;
    for (auto r : rows) {
      row_idx.push_back(r.GetRect().row());
    }
    EditRenderContext<RC> ctx{parent_context, parent_context->color, nullptr,
                              (this->*compute_row_offset)(
                                  parent_context->window->height(), first_drawn,
                                  row_idx, &parent_context->animating)};
    renderer.Draw(&ctx);
  }
#endif

  Site* const site_;
  int cursor_row_ = 0;  // cursor row as an offset into the view buffer
  ID cursor_ = AnnotatedString::Begin();
  ID selection_anchor_ = ID();
  EditNotification state_;
  CommandSet unpublished_commands_;
  CommandSet unacknowledged_commands_;
  BufferRef active_side_buffer_;
  AnnotationEditor ed_;

#if 0
  int RenderCommon(int window_height, LineCallback callback);
  int MainRowOfs(int window_height, int first_draw_ignored,
                 const std::vector<int>& rows, bool* animating) {
    // out_row = buf_row + ofs
    // => cursor_row_ = cursor_row.Rect().row() + ofs
    // => ofs = cursor_row_ - cursor_row.Rect().row()
    return cursor_row_ - (rows.empty() ? 0 : rows[0]);
  }
  int RenderSideBarCommon(int window_height, LineCallback callback);
  int SideBarRowOfs(int window_height, int first_drawn,
                    const std::vector<int>& rows_ignored, bool* animating) {
    int nlines = 0;
    state_.side_buffers.ForEachValue(
        active_side_buffer_.name,
        [&](const SideBuffer& buf) { nlines += buf.line_ofs.size() - 1; });
    std::vector<int> lines = active_side_buffer_.lines;
    int ideal_pos = -1;
    if (!lines.empty()) {
      ideal_pos = window_height / 2 -
                  std::accumulate(lines.begin(), lines.end(), 0) / lines.size();
      if (ideal_pos > 0)
        ideal_pos = 0;
      else if (ideal_pos < window_height - 1 - nlines)
        ideal_pos = window_height - 1 - nlines;
      if (ideal_pos != last_sb_offset_) {
        int dir = ideal_pos < last_sb_offset_ ? -1 : 1;
        int mag = abs(ideal_pos - last_sb_offset_) / 10;
        if (mag < 1) mag = 1;
        last_sb_offset_ += mag * dir;
        *animating = true;
      }
    }
    Log() << "nlines:" << nlines << " last_sb_offset:" << last_sb_offset_
          << " lines:" << absl::StrJoin(lines, ",") << " ideal:" << ideal_pos
          << " first_drawn:" << first_drawn
          << " window_height:" << window_height
          << " return_ofs:" << last_sb_offset_ + first_drawn;
    return last_sb_offset_ + first_drawn;
  }
#endif
};
