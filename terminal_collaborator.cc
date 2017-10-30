#include "terminal_collaborator.h"
#include <deque>
#include <vector>

namespace ced {
namespace buffer {

TerminalCollaborator::TerminalCollaborator(tl::Fullscreen* terminal)
    : terminal_(terminal), cursor_(ElemString::Begin()) {}

void TerminalCollaborator::Push(const EditNotification& notification) {
  {
    absl::MutexLock lock(&mu_);
    last_seen_views_ = notification.views;
  }
  terminal_->Invalidate();
}

EditResponse TerminalCollaborator::Pull() {
  EditResponse r;
  r.done = true;
  return r;
}

static bool is_nl(const Elem* elem) {
  const char* c = absl::any_cast<char>(elem);
  if (!c) return false;
  return *c == '\n';
}

void TerminalCollaborator::Render(tl::FrameBuffer* fb) {
  absl::MutexLock lock(&mu_);
  const ElemString* s = last_seen_views_.Lookup("main");
  if (!s) return;

  // generate a deque of lines by the ID of the starting element
  std::deque<woot::ID> lines;
  ElemString::Iterator it(*s, cursor_);
  while (!it.is_visible() && !it.is_begin()) {
    it.MovePrev();
  }
  while (!it.is_visible() && !it.is_end()) {
    it.MoveNext();
  }
  cursor_ = it.id();
  while (!it.is_begin() && lines.size() < fb->rows()) {
    if (it.is_visible() && is_nl(it.value())) {
      lines.push_front(it.id());
    }
    it.MovePrev();
  }
  if (it.is_begin()) {
    lines.push_front(it.id());
  }
  int cursor_line_idx = static_cast<int>(lines.size()) - 1;
  it = ElemString::Iterator(*s, cursor_);
  if (!it.is_end()) it.MoveNext();
  int tgt_lines = lines.size() + fb->rows();
  while (!it.is_end() && lines.size() < tgt_lines) {
    if (it.is_visible() && is_nl(it.value())) {
      lines.push_back(it.id());
    }
    it.MoveNext();
  }

  // start rendering
  if (cursor_line_idx < cursor_row_) {
    cursor_row_ = cursor_line_idx;
  }

  for (int row = 0; row < fb->rows(); row++) {
    int col = 0;
    int rend_row = row - cursor_row_ + cursor_line_idx;
    assert(rend_row >= 0);
    if (rend_row >= lines.size()) break;
    it = ElemString::Iterator(*s, lines[rend_row]);
    it.MoveNext();
    for (;;) {
      if (it.id() == cursor_) {
        fb->set_cursor_pos(col, row);
      }
      if (it.is_end()) break;
      if (it.is_visible()) {
        if (const char* c = absl::any_cast<char>(it.value())) {
          if (*c == '\n') break;
          fb->cell(col, row)->set_glyph(*c);
          col++;
        }
      }
      it.MoveNext();
    }
  }
}

void TerminalCollaborator::ProcessKey(tl::Key key) {
  absl::MutexLock lock(&mu_);
  const ElemString* s = last_seen_views_.Lookup("main");
  if (!s) return;

  if (key.is_function()) {
    switch (key.function()) {
      default:
        break;
      case tl::Key::LEFT: {
        ElemString::Iterator it(*s, cursor_);
        do {
          if (it.is_begin()) break;
          it.MovePrev();
        } while (!it.is_visible());
        terminal_->Invalidate();
      } break;
      case tl::Key::RIGHT: {
        ElemString::Iterator it(*s, cursor_);
        do {
          if (it.is_begin()) break;
          it.MovePrev();
        } while (!it.is_visible());
        terminal_->Invalidate();
      } break;
    }
  } else if (key.mods() == 0) {
  }
}

}  // namespace buffer
}  // namespace ced
