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
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

std::vector<std::string> Editor::DebugData() const {
  std::vector<std::string> r;
  r.push_back(absl::StrCat("cursor_line ", cursor_line_.value()));
  r.push_back(absl::StrCat("nrow_before_sub ", debug_.nrow_before_sub));
  r.push_back(absl::StrCat("window_height ", debug_.window_height));
  r.push_back(absl::StrCat("cursor ", absl::Hex(cursor_.id, absl::kZeroPad16)));
  r.push_back(absl::StrCat("cursor_reported ",
                           absl::Hex(cursor_reported_.id, absl::kZeroPad16)));
  r.push_back(absl::StrCat("selection_anchor ",
                           absl::Hex(selection_anchor_.id, absl::kZeroPad16)));
  auto add_proto = [&r](const std::string& name,
                        const google::protobuf::Message& msg) {
    r.push_back(absl::StrCat(name, ":"));
    auto dbg = msg.DebugString();
    for (auto line : absl::StrSplit(dbg, '\n')) {
      r.push_back(absl::StrCat("  ", line));
    }
  };
  add_proto("unpublished_commands", unpublished_commands_);
  add_proto("unacknowledged_commands", unacknowledged_commands_);
  return r;
}

EditResponse Editor::MakeResponse() {
  PublishCursor();

  Log() << "EDITOR: " << name_ << " done:" << state_.shutdown;

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
  Log() << "EDITOR: " << name_ << " UpdateState shutdown=" << state.shutdown;

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

void Editor::Copy(Renderer* d) {
  if (SelectMode()) {
    d->ClipboardPut(state_.content.Render(cursor_, selection_anchor_));
  }
}

void Editor::Cut(Renderer* d) {
  if (SelectMode()) {
    d->ClipboardPut(state_.content.Render(cursor_, selection_anchor_));
    DeleteSelection();
    SetSelectMode(false);
  }
}

void Editor::Paste(Renderer* d) {
  if (selection_anchor_ != ID()) {
    DeleteSelection();
    SetSelectMode(false);
  }
  cursor_ = state_.content.Insert(&unpublished_commands_, site_,
                                  d->ClipboardGet(), cursor_);
}

void Editor::InsChar(char c) {
  DeleteSelection();
  SetSelectMode(false);
  cursor_ = state_.content.Insert(&unpublished_commands_, site_,
                                  absl::string_view(&c, 1), cursor_);
  ChangeCursorLine(c == '\n');
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
  ChangeCursorLine(-(it.value() == '\n'));
  it.MovePrev();
  cursor_ = it.id();
}

void Editor::CursorRight() {
  AnnotatedString::Iterator it(state_.content, cursor_);
  it.MoveNext();
  ChangeCursorLine(it.value() == '\n');
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
  ChangeCursorLine(1);
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
  ChangeCursorLine(-1);
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

void Editor::Render(Theme* theme, Widget* parent) {
  Widget* content = parent->MakeContent(
      Widget::Options().set_id(name_).set_activatable(editable_));

  if (content->Focus()) {
    if (auto c = content->CharPressed()) {
      InsChar(c);
    } else if (content->Chord("up")) {
      MoveUp();
    } else if (content->Chord("down")) {
      MoveDown();
    } else if (content->Chord("left")) {
      MoveLeft();
    } else if (content->Chord("right")) {
      MoveRight();
    } else if (content->Chord("home")) {
      MoveStartOfLine();
    } else if (content->Chord("end")) {
      MoveEndOfLine();
    } else if (content->Chord("S-up")) {
      SelectUp();
    } else if (content->Chord("S-down")) {
      SelectDown();
    } else if (content->Chord("del")) {
      Backspace();
    } else if (content->Chord("C-c")) {
      Copy(content->renderer());
    } else if (content->Chord("C-v")) {
      Paste(content->renderer());
    } else if (content->Chord("C-x")) {
      Cut(content->renderer());
    } else if (content->Chord("ret")) {
      InsChar('\n');
    }
  }

  auto* r = parent->renderer();
  auto ex = r->extents();
  r->solver()->add_constraints(
      {rhea::constraint(cursor_line_ == cursor_line_.value(),
                        rhea::strength::strong()),
       rhea::constraint(
           content->right() - content->left() >= 80 * ex.chr_width,
           editable_ ? rhea::strength::strong() : rhea::strength::weak()),
       rhea::constraint(content->bottom() - content->top() >= 3 * ex.chr_height,
                        rhea::strength::weak()),
       cursor_line_ * ex.chr_height >= 0,
       cursor_line_ * ex.chr_height <=
           parent->bottom() - parent->top() - ex.chr_height});

  ID cursor = cursor_ = AnnotatedString::Iterator(state_.content, cursor_).id();
  AnnotatedString::LineIterator line_cr(state_.content, cursor_);
  rhea::variable cursor_line = cursor_line_;
  if (!editable_) cursor = ID();
  content->Draw(
      [line_cr, cursor_line, cursor, ex, theme, content](DeviceContext* ctx) {
        ctx->Fill(0, 0, ctx->width(), ctx->height(),
                  theme->ThemeToken({}, 0).background);
        float cl = cursor_line.value() * ex.chr_height;
        ctx->Fill(0, cl, ctx->width(), cl + ex.chr_height,
                  theme->ThemeToken({}, Theme::HIGHLIGHT_LINE).background);
        AnnotatedString::LineIterator line_bk = line_cr;
        AnnotatedString::LineIterator line_fw = line_cr;
        RenderLine(ctx, ex, theme, cursor, cl, line_cr, true);
        for (int i = 1; i <= ctx->height() / ex.chr_height; i++) {
          if (line_bk.MovePrev()) {
            RenderLine(ctx, ex, theme, cursor, cl - i * ex.chr_height, line_bk,
                       false);
          }
          if (line_fw.MoveNext()) {
            RenderLine(ctx, ex, theme, cursor, cl + i * ex.chr_height, line_fw,
                       false);
          }
        }
      });
}

typedef absl::InlinedVector<std::string, 2> GutterVec;

struct CharDet {
  CharDet(uint32_t base_flags, GutterVec* g)
      : chr_flags(base_flags), gutters(g) {}
  bool has_diagnostic = false;
  uint32_t chr_flags;
  std::vector<std::string> tags;
  GutterVec* const gutters;

  void AddGutter(std::string&& s) {
    for (const auto& g : *gutters) {
      if (s == g) return;
    }
    gutters->emplace_back(std::move(s));
  }

  void FillFromIterator(AnnotatedString::AllIterator it) {
    it.ForEachAttrValue([&](const Attribute& attr) {
      switch (attr.data_case()) {
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
                AddGutter(absl::StrCat("@", attr.size().size(), ".",
                                       attr.size().bits()));
              } else {
                AddGutter(absl::StrCat("@", attr.size().size()));
              }
              break;
            case SizeAnnotation::SIZEOF_SELF:
              if (attr.size().bits()) {
                AddGutter(absl::StrCat(
                    attr.size().size() * 8 + attr.size().bits(), "b"));
              } else {
                AddGutter(absl::StrCat(attr.size().size(), "B"));
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
  }
};

void Editor::RenderLine(DeviceContext* ctx, const Device::Extents& extents,
                        Theme* theme, ID cursor, int y,
                        AnnotatedString::LineIterator lit, bool highlight) {
  AnnotatedString::AllIterator it = lit.AsAllIterator();
  const uint32_t base_flags = highlight ? Theme::HIGHLIGHT_LINE : 0;
  GutterVec gutter_annotations;
  std::vector<DeviceContext::TextElem> to_print;
  CharFmt base_fmt = theme->ThemeToken({}, 0);
  while (it.id() != AnnotatedString::End()) {
    if (it.is_visible() || it.is_begin()) {
      CharDet cd(base_flags, &gutter_annotations);
      cd.FillFromIterator(it);
      if (cd.has_diagnostic) {
        cd.tags.push_back("invalid");
      }
      if (it.is_visible() && it.id() != lit.id()) {
        if (it.value() == '\n') {
          if (it.id() == cursor) {
            ctx->PutCaret(0, y + extents.chr_height,
                          CARET_PRIMARY | CARET_BLINKING,
                          theme->ThemeToken(cd.tags, Theme::CARET).foreground);
          }
          break;
        } else {
          char c = it.value();
          auto fmt = theme->ThemeToken(cd.tags, cd.chr_flags);
          if (fmt.background != base_fmt.background) {
            ctx->Fill(to_print.size() * extents.chr_width, y, (to_print.size()+1) * extents.chr_width, y + extents.chr_height, fmt.background);
          }
          to_print.emplace_back(DeviceContext::TextElem{
              static_cast<uint32_t>(c), fmt.foreground, fmt.highlight});
        }
      }
      if (it.id() == cursor) {
        ctx->PutCaret(to_print.size() * extents.chr_width, y,
                      CARET_PRIMARY | CARET_BLINKING,
                      theme->ThemeToken(cd.tags, Theme::CARET).foreground);
      }
    }
    it.MoveNext();
  }
  ctx->PutText(0, y, to_print.data(), to_print.size());
  std::string gutter = absl::StrJoin(gutter_annotations, ",");
  int x = ctx->width() - gutter.length() * extents.chr_width;
  CharFmt fill_attr =
      theme->ThemeToken(::Theme::Tag{"comment.gutter"}, base_flags);
  ctx->Fill(x, y, ctx->width(), y + extents.chr_height, fill_attr.background);
  ctx->PutText(x, y, gutter.data(), gutter.length(), fill_attr.foreground,
               fill_attr.highlight);
}
