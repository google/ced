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
#include "editor.h"

EditResponse Editor::MakeResponse() {
  PublishCursor();

  EditResponse r;
  r.done = state_.shutdown;
  r.become_used = !unpublished_commands_.commands().empty();
  state_.content = state_.content.Integrate(unpublished_commands_);
  r.content_updates.MergeFrom(unpublished_commands_);
  unacknowledged_commands_.MergeFrom(unpublished_commands_);
  unpublished_commands_.Clear();
  assert(unpublished_commands_.commands().empty());
  return r;
}

void Editor::PublishCursor() {
  AnnotationEditor::ScopedEdit edit(&ed_, &unpublished_commands_);
  Attribute curs;
  curs.mutable_cursor();
  ed_.Mark(cursor_,
           AnnotatedString::Iterator(state_.content, cursor_).Next().id(),
           curs);
  if (selection_anchor_ != ID()) {
    Attribute sel;
    sel.mutable_selection();
    if (state_.content.OrderIDs(cursor_, selection_anchor_) < 0) {
      ed_.Mark(cursor_, selection_anchor_, sel);
    } else {
      ed_.Mark(selection_anchor_, cursor_, sel);
    }
  }
  AnnotatedString::Iterator(state_.content, cursor_)
      .ForEachAttrValue([this, curs](const Attribute& attr) {
        if (attr.data_case() != Attribute::kBufferRef) return;
        auto it = buffers_.find(attr.buffer_ref().buffer());
        if (it == buffers_.end()) return;
        CommandSet cmds;
        auto cur_content = it->second.buffer->ContentSnapshot();
        {
          AnnotationEditor::ScopedEdit edit_child(&it->second.ed, &cmds);
          for (auto line : attr.buffer_ref().lines()) {
            Log() << "Mark child frame line " << line;
            AnnotatedString::LineIterator li =
                AnnotatedString::LineIterator::FromLineNumber(cur_content,
                                                              line);
            it->second.ed.Mark(li.id(), li.Next().id(), curs);
          }
        }
        Log() << "Push changes " << cmds.DebugString();
        it->second.buffer->PushChanges(&cmds, true);
        Log() << "Result: "
              << it->second.buffer->ContentSnapshot().AsProto().DebugString();
      });
  cursor_reported_ = cursor_;
}

void Editor::UpdateState(LogTimer* tmr, const EditNotification& state) {
  state_ = state;
  auto s2 = state_.content;
  CommandSet unacked;
  for (const auto& cmd : unacknowledged_commands_.commands()) {
    auto s3 = s2;
    s2.Integrate(cmd);
    if (!s3.SameTotalIdentity(s2)) {
      *unacked.add_commands() = cmd;
      tmr->Mark("unacked");
    } else {
      tmr->Mark("acked");
    }
  }
  unacknowledged_commands_.Swap(&unacked);

  std::map<ID, BufferInfo> new_buffers;
  state_.content.ForEachAttribute(
      Attribute::kBuffer, [this, &new_buffers](ID id, const Attribute& attr) {
        auto it = buffers_.find(id);
        if (it != buffers_.end()) {
          new_buffers.emplace(it->first, std::move(it->second));
          buffers_.erase(it);
        } else {
          AnnotatedString s;
          s.Insert(site_, attr.buffer().contents(), AnnotatedString::Begin());
          new_buffers.emplace(id,
                              BufferInfo{Buffer::Builder()
                                             .SetFilename(attr.buffer().name())
                                             .SetInitialString(s)
                                             .SetSynthetic()
                                             .Make(),
                                         AnnotationEditor(site_)});
        }
      });
  buffers_.swap(new_buffers);
  for (auto& b : new_buffers) {
    auto* p = b.second.buffer.release();
    // buffer destruction can be slow and mutex-grabby... just do it in its own
    // thread
    std::thread([p]() { delete p; }).detach();
  }
}

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
  state_.content.MakeDelete(&unpublished_commands_, cursor_);
  AnnotatedString::Iterator it(state_.content, cursor_);
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
  cursor_ = state_.content.Insert(&unpublished_commands_, site_, env->clipboard,
                                  cursor_);
}

void Editor::InsChar(char c) {
  DeleteSelection();
  SetSelectMode(false);
  cursor_ = state_.content.Insert(&unpublished_commands_, site_,
                                  absl::string_view(&c, 1), cursor_);
  cursor_row_ += (c == '\n');
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
  state_.content.MakeDelete(&unpublished_commands_, cursor_, selection_anchor_);
  AnnotatedString::Iterator it(state_.content, cursor_);
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::CursorLeft() {
  AnnotatedString::Iterator it(state_.content, cursor_);
  cursor_row_ -= it.value() == '\n';
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::CursorRight() {
  AnnotatedString::Iterator it(state_.content, cursor_);
  it.MoveNext();
  cursor_row_ += it.value() == '\n';
  cursor_ = it.id();
}

void Editor::CursorDown() {
  AnnotatedString::Iterator it(state_.content, cursor_);
  int col = 0;
  auto edge = [&it]() {
    return it.is_begin() || it.is_end() || it.value() == '\n';
  };
  while (!edge()) {
    it.MovePrev();
    col++;
  }
  Log() << "col:" << col;
  it = AnnotatedString::Iterator(state_.content, cursor_);
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
  AnnotatedString::Iterator it(state_.content, cursor_);
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
  cursor_ = AnnotatedString::LineIterator(state_.content, cursor_).id();
}

void Editor::CursorEndOfLine() {
  cursor_ = AnnotatedString::LineIterator(state_.content, cursor_)
                .Next()
                .AsIterator()
                .Prev()
                .id();
}
