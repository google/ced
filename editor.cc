#include "editor.h"

void Editor::SelectLeft() {
  SetSelectMode(true);
  CursorLeft();
}

void Editor::MoveLeft() {
  SetSelectMode(false);
  CursorLeft();
}

void Editor::SelectRight() {
  SetSelectMode(true);
  CursorRight();
}

void Editor::MoveRight() {
  SetSelectMode(false);
  CursorRight();
}

void Editor::MoveStartOfLine() {
  SetSelectMode(false);
  CursorStartOfLine();
}

void Editor::MoveEndOfLine() {
  SetSelectMode(false);
  CursorEndOfLine();
}

void Editor::MoveDownN(int n) {
  SetSelectMode(false);
  for (int i = 0; i < n; i++) CursorDown();
}

void Editor::MoveUpN(int n) {
  SetSelectMode(false);
  for (int i = 0; i < n; i++) CursorUp();
}

void Editor::SelectDownN(int n) {
  SetSelectMode(true);
  for (int i = 0; i < n; i++) CursorDown();
}

void Editor::SelectUpN(int n) {
  SetSelectMode(true);
  for (int i = 0; i < n; i++) CursorUp();
}

void Editor::Backspace() {
  SetSelectMode(false);
  NextRenderMustNotHaveID(cursor_);
  state_.content.MakeRemove(&commands_, cursor_);
  String::Iterator it(state_.content, cursor_);
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::Copy(AppEnv* env) {
  if (SelectMode()) {
    env->clipboard = state_.content.Render(cursor_, selection_anchor_);
  }
}

void Editor::Cut(AppEnv* env) {
  if (SelectMode()) {
    env->clipboard = state_.content.Render(cursor_, selection_anchor_);
    DeleteSelection();
    SetSelectMode(false);
  }
}

void Editor::Paste(AppEnv* env) {
  if (selection_anchor_ != ID()) {
    DeleteSelection();
    SetSelectMode(false);
  }
  cursor_ =
      state_.content.MakeInsert(&commands_, site_, env->clipboard, cursor_);
  NextRenderMustHaveID(cursor_);
}

void Editor::InsChar(char c) {
  DeleteSelection();
  SetSelectMode(false);
  cursor_ = state_.content.MakeInsert(&commands_, site_, c, cursor_);
  cursor_row_ += (c == '\n');
  NextRenderMustHaveID(cursor_);
}

void Editor::SetSelectMode(bool sel) {
  if (!sel) {
    selection_anchor_ = ID();
  } else if (selection_anchor_ == ID()) {
    selection_anchor_ = cursor_;
  }
}

void Editor::DeleteSelection() {
  if (!SelectMode()) return;
  NextRenderMustNotHaveID(cursor_);
  state_.content.MakeRemove(&commands_, cursor_, selection_anchor_);
  String::Iterator it(state_.content, cursor_);
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::CursorLeft() {
  String::Iterator it(state_.content, cursor_);
  cursor_row_ -= it.value() == '\n';
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::CursorRight() {
  String::Iterator it(state_.content, cursor_);
  it.MoveNext();
  cursor_row_ += it.value() == '\n';
  cursor_ = it.id();
}

void Editor::CursorDown() {
  String::Iterator it(state_.content, cursor_);
  int col = 0;
  auto edge = [&it]() {
    return it.is_begin() || it.is_end() || it.value() == '\n';
  };
  while (!edge()) {
    it.MovePrev();
    col++;
  }
  Log() << "col:" << col;
  it = String::Iterator(state_.content, cursor_);
  do {
    it.MoveNext();
  } while (!edge());
  it.MoveNext();
  for (; col > 0 && !edge(); col--) {
    it.MoveNext();
  }
  it.MovePrev();
  cursor_ = it.id();
  cursor_row_++;
}

void Editor::CursorUp() {
  String::Iterator it(state_.content, cursor_);
  int col = 0;
  auto edge = [&it]() {
    return it.is_begin() || it.is_end() || it.value() == '\n';
  };
  while (!edge()) {
    it.MovePrev();
    col++;
  }
  Log() << "col:" << col;
  do {
    it.MovePrev();
  } while (!edge());
  it.MoveNext();
  for (; col > 0 && !edge(); col--) {
    it.MoveNext();
  }
  it.MovePrev();
  cursor_ = it.id();
  cursor_row_--;
}

void Editor::CursorStartOfLine() {
  cursor_ = String::LineIterator(state_.content, cursor_).id();
}

void Editor::CursorEndOfLine() {
  cursor_ = String::LineIterator(state_.content, cursor_)
                .Next()
                .AsIterator()
                .Prev()
                .id();
}

void Editor::NextRenderMustHaveID(ID id) {
  check_most_recent_edit_ = [id](const EditNotification& state) {
    return state.content.Has(id);
  };
}

void Editor::NextRenderMustNotHaveID(ID id) {
  check_most_recent_edit_ = [id](const EditNotification& state) {
    return !String::AllIterator(state.content, id).is_visible();
  };
}

void Editor::RenderCommon(
    int window_height,
    std::function<void(LineInfo, const std::vector<CharInfo>&)> add_line) {
  if (cursor_row_ < 0) {
    cursor_row_ = 0;
  } else if (cursor_row_ >= window_height) {
    cursor_row_ = window_height - 1;
  }

  cursor_ = String::Iterator(state_.content, cursor_).id();
  String::LineIterator line_cr(state_.content, cursor_);
  String::LineIterator line_end_cr = line_cr.Next();
  String::LineIterator line_bk = line_cr;
  String::LineIterator line_fw = line_cr;
  for (int i = 0; i < window_height; i++) {
    line_bk.MovePrev();
    line_fw.MoveNext();
  }
  String::AllIterator it = line_bk.AsAllIterator();
  AnnotationTracker<Tag> t_token(state_.token_types);
  AnnotationTracker<ID> t_diagnostic(state_.diagnostic_ranges);
  AnnotationTracker<SideBufferRef> t_side_buffer_ref(state_.side_buffer_refs);
  bool in_selection = false;
  std::vector<CharInfo> ci;
  int nrow = 0;
  int ncol = 0;
  while (it.id() != line_fw.id()) {
    t_token.Enter(it.id());
    t_diagnostic.Enter(it.id());
    t_side_buffer_ref.Enter(it.id());
    if (SelectMode() && it.id() == selection_anchor_) {
      in_selection = !in_selection;
    }
    if (it.id() == cursor_) {
      if (SelectMode()) in_selection = !in_selection;
      cursor_token_ = t_token.cur();
      if (!t_side_buffer_ref.cur().name.empty()) {
        active_side_buffer_ = t_side_buffer_ref.cur();
      } else {
        active_side_buffer_.lines.clear();
      }
    }

    if (it.is_visible()) {
      if (it.value() == '\n') {
        LineInfo li{it.id() == line_end_cr.id()};
        nrow++;
        ncol = 0;
        add_line(li, ci);
        ci.clear();
      } else {
        ncol++;
        Tag tok = t_token.cur();
        if (t_diagnostic.cur() != ID()) {
          tok = tok.Push("error");
        }
        ci.emplace_back(
            CharInfo{it.value(), in_selection, it.id() == cursor_, tok});
      }
    }
    it.MoveNext();
  }
}

void Editor::RenderSideBarCommon(int window_height, LineCallback callback) {
  state_.side_buffers.ForEachValue(
      active_side_buffer_.name, [&](const SideBuffer& buf) {
        std::vector<CharInfo> ci;
        for (size_t line = 0; line < buf.line_ofs.size() - 1; line++) {
          size_t line_beg = buf.line_ofs[line];
          size_t line_end = buf.line_ofs[line + 1];
          ci.clear();
          ci.reserve(line_end - line_beg);
          for (size_t i = line_beg; i < line_end; i++) {
            ci.emplace_back(
                CharInfo{buf.content[i] == '\n' ? ' ' : buf.content[i], false,
                         false, Tag()});
          }
          callback(LineInfo{std::find(active_side_buffer_.lines.begin(),
                                      active_side_buffer_.lines.end(),
                                      line) != active_side_buffer_.lines.end()},
                   ci);
        }
      });
}
