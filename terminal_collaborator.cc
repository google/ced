#include "terminal_collaborator.h"
#include <curses.h>
#include <deque>
#include <vector>

TerminalCollaborator::TerminalCollaborator(
    const std::function<void()> invalidate)
    : invalidate_(invalidate), cursor_(ElemString::Begin()), cursor_row_(0) {}

void TerminalCollaborator::Push(const EditNotification& notification) {
  {
    absl::MutexLock lock(&mu_);
    content_ = notification.content;
  }
  invalidate_();
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

void TerminalCollaborator::Render() {
  absl::MutexLock lock(&mu_);

  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);

  // generate a deque of lines by the ID of the starting element
  std::deque<ID> lines;
  ElemString::Iterator it(content_, cursor_);
  while (!it.is_visible() && !it.is_begin()) {
    it.MovePrev();
  }
  while (!it.is_visible() && !it.is_end()) {
    it.MoveNext();
  }
  cursor_ = it.id();
  while (!it.is_begin() && lines.size() < fb_rows) {
    if (it.is_visible() && is_nl(it.value())) {
      lines.push_front(it.id());
    }
    it.MovePrev();
  }
  if (it.is_begin()) {
    lines.push_front(it.id());
  }
  int cursor_line_idx = static_cast<int>(lines.size()) - 1;
  it = ElemString::Iterator(content_, cursor_);
  if (!it.is_end()) it.MoveNext();
  int tgt_lines = lines.size() + fb_rows;
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

  int cursor_row = 0;
  int cursor_col = 0;

  for (int row = 0; row < fb_rows; row++) {
    int col = 0;
    int rend_row = row - cursor_row_ + cursor_line_idx;
    assert(rend_row >= 0);
    if (rend_row >= lines.size()) break;
    it = ElemString::Iterator(content_, lines[rend_row]);
    if (it.id() == cursor_) {
      cursor_row = row;
      cursor_col = col;
    }
    it.MoveNext();
    for (;;) {
      if (it.id() == cursor_) {
        cursor_row = row;
        cursor_col = col;
      }
      if (it.is_end()) break;
      if (it.is_visible()) {
        if (const char* c = absl::any_cast<char>(it.value())) {
          if (*c == '\n') break;
          mvaddch(row, col, *c);
          col++;
        }
      }
      it.MoveNext();
    }
  }

  move(cursor_row, cursor_col);
}

void TerminalCollaborator::ProcessKey(int key) {
  absl::MutexLock lock(&mu_);

  switch (key) {
    case KEY_LEFT: {
      ElemString::Iterator it(content_, cursor_);
      do {
        if (it.is_begin()) break;
        it.MovePrev();
      } while (!it.is_visible());
      cursor_ = it.id();
      invalidate_();
    } break;
    case KEY_RIGHT: {
      ElemString::Iterator it(content_, cursor_);
      do {
        if (it.is_end()) break;
        it.MoveNext();
      } while (!it.is_visible());
      cursor_ = it.id();
      invalidate_();
    } break;
  }
}
