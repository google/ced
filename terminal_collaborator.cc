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
#include "terminal_collaborator.h"
#include <curses.h>
#include <deque>
#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "log.h"

constexpr char ctrl(char c) { return c & 0x1f; }

TerminalCollaborator::TerminalCollaborator(const Buffer* buffer,
                                           std::function<void()> invalidate)
    : Collaborator("terminal", absl::Seconds(0)),
      invalidate_(invalidate),
      used_([this]() {
        mu_.AssertHeld();
        recently_used_ = true;
      }),
      recently_used_(false),
      cursor_(String::Begin()),
      selection_anchor_(String::Begin()),
      cursor_row_(0),
      sb_cursor_row_(0) {}

void TerminalCollaborator::Push(const EditNotification& notification) {
  {
    absl::MutexLock lock(&mu_);
    state_ = notification;
  }
  invalidate_();
}

EditResponse TerminalCollaborator::Pull() {
  EditResponse r;

  auto ready = [this]() {
    mu_.AssertHeld();
    return state_.shutdown || !commands_.empty() || recently_used_;
  };

  mu_.LockWhen(absl::Condition(&ready));
  r.done = state_.shutdown;
  r.become_used = recently_used_ || !commands_.empty();
  commands_.swap(r.content);
  assert(commands_.empty());
  recently_used_ = false;
  mu_.Unlock();

  return r;
}

static const char* SeverityString(Severity sev) {
  switch (sev) {
    case Severity::UNSET:
      return "???";
    case Severity::IGNORED:
      return "IGNORED";
    case Severity::NOTE:
      return "NOTE";
    case Severity::WARNING:
      return "WARNING";
    case Severity::ERROR:
      return "ERROR";
    case Severity::FATAL:
      return "FATAL";
  }
  return "XXXXX";
}

std::pair<ID, ID> TerminalCollaborator::SelRange() const {
  if (selection_anchor_ == ID()) {
    return std::make_pair(String::Begin(), String::Begin());
  }
  auto p = std::make_pair(cursor_, selection_anchor_);
  if (state_.content.OrderIDs(p.first, p.second) > 0) {
    std::swap(p.first, p.second);
  }
  return p;
}

void TerminalCollaborator::Render(TerminalColor* color,
                                  absl::Time last_key_press) {
  auto ready = [this]() {
    mu_.AssertHeld();
    return state_.shutdown || state_.content.Has(cursor_);
  };

  mu_.LockWhen(absl::Condition(&ready));

  if (state_.shutdown) {
    mu_.Unlock();
    return;
  }

  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);

  if (cursor_row_ >= fb_rows) {
    cursor_row_ = fb_rows - 1;
  } else if (cursor_row_ < 0) {
    cursor_row_ = 0;
  }

  Log() << "cursor_row_:" << cursor_row_;

  String::LineIterator line_it(state_.content, cursor_);
  for (int i = 0; i < cursor_row_; i++) line_it.MovePrev();

  int cursor_row = 0;
  int cursor_col = 0;
  Token cursor_token;

  String::AllIterator it(state_.content, line_it.id());
  AnnotationTracker<Token> t_token(state_.token_types);
  AnnotationTracker<ID> t_diagnostic(state_.diagnostic_ranges);
  AnnotationTracker<SideBufferRef> t_side_buffer_ref(state_.side_buffer_refs);
  bool in_selection = false;
  for (int row = 0; row < fb_rows; row++) {
    int col = 0;
    auto move_next = [&]() {
      mu_.AssertHeld();
      if (selection_anchor_ != ID() && it.id() == selection_anchor_) {
        in_selection = !in_selection;
      }
      if (it.id() == cursor_) {
        if (selection_anchor_ != ID()) {
          in_selection = !in_selection;
        }
        cursor_row = row;
        cursor_col = col;
        cursor_token = t_token.cur();
        if (!t_side_buffer_ref.cur().name.empty()) {
          active_side_buffer_ = t_side_buffer_ref.cur();
        } else {
          active_side_buffer_.lines.clear();
        }
      }
      it.MoveNext();
      t_token.Enter(it.id());
      t_diagnostic.Enter(it.id());
      t_side_buffer_ref.Enter(it.id());
    };
    do {
      move_next();
    } while (!it.is_end() && !it.is_visible());
    uint32_t flags = 0;
    if (row == cursor_row_) flags |= Theme::HIGHLIGHT_LINE;
    for (;;) {
      if (it.is_end()) {
        row = fb_rows;
        break;
      }
      if (it.is_visible()) {
        if (it.value() == '\n') break;
        Token tok = t_token.cur();
        if (t_diagnostic.cur() != ID()) {
          tok = tok.Push("error");
        }
        uint32_t chr_flags = flags;
        if (in_selection) chr_flags |= Theme::SELECTED;
        if (col < 80) {
          mvaddch(row, col, it.value() | color->Theme(tok, chr_flags));
        }
        col++;
      }
      move_next();
    }
    for (; col < 80; col++) {
      mvaddch(row, col, ' ' | color->Theme(Token(), flags));
    }
  }

  if (fb_cols >= 100) {
    int max_length = fb_cols - 82;
    int row = 0;
    state_.diagnostics.ForEachValue([&](const Diagnostic& diagnostic) {
      if (row >= fb_rows) return;
      std::string msg = absl::StrCat(diagnostic.index, ": [",
                                     SeverityString(diagnostic.severity), "] ",
                                     diagnostic.message);
      if (msg.length() > max_length) {
        msg.resize(max_length);
      }
      mvaddstr(row++, 82, msg.c_str());
    });

    cursor_token.ForEach([&](const std::string& msg) {
      if (row >= fb_rows) return;
      mvaddstr(row++, 82, msg.c_str());
    });

    state_.side_buffers.ForEachValue(
        active_side_buffer_.name, [&](const SideBuffer& buffer) {
          mu_.AssertHeld();
          if (row >= fb_rows) return;
          if (!active_side_buffer_.lines.empty()) {
            Log() << "sb_cursor_row_=" << sb_cursor_row_
                  << " line[0]=" << active_side_buffer_.lines.front()
                  << " line[-1]=" << active_side_buffer_.lines.back();
            if (sb_cursor_row_ >= buffer.line_ofs.size()) {
              sb_cursor_row_ = buffer.line_ofs.size() - 1;
            }
            int adj = 0;
            if (active_side_buffer_.lines.front() < sb_cursor_row_) {
              sb_cursor_row_ = active_side_buffer_.lines.front();
              adj++;
            }
            if (active_side_buffer_.lines.back() >= sb_cursor_row_ + fb_rows) {
              sb_cursor_row_ = active_side_buffer_.lines.back() - fb_rows;
              adj++;
            }
            if (adj == 2) {
              sb_cursor_row_ =
                  (buffer.line_ofs.front() + buffer.line_ofs.back()) / 2 -
                  fb_rows / 2;
            }
            if (sb_cursor_row_ < 0) {
              sb_cursor_row_ = 0;
            }
          }
          int col = 0;
          int sbrow = sb_cursor_row_;
          for (int i = buffer.line_ofs[sb_cursor_row_];
               i < buffer.content.size(); i++) {
            uint32_t flags = 0;
            if (std::find(active_side_buffer_.lines.begin(),
                          active_side_buffer_.lines.end(),
                          sbrow) != active_side_buffer_.lines.end()) {
              flags |= Theme::HIGHLIGHT_LINE;
            }
            char c = buffer.content[i];
            if (c == '\n') {
              chtype fill = ' ' | color->Theme(Token(), flags);
              while (col < fb_cols) {
                mvaddch(row, 82 + (col++), fill);
              }
              row++;
              sbrow++;
              col = 0;
              if (row >= fb_rows) break;
            } else if (col < fb_cols) {
              mvaddch(row, 82 + (col++), c | color->Theme(Token(), flags));
            }
          }
        });
  }

  auto frame_time = absl::Now() - last_key_press;
  std::ostringstream out;
  out << frame_time;
  std::string ftstr = out.str();
  mvaddstr(fb_rows - 1, fb_cols - ftstr.length(), ftstr.c_str());

  mu_.Unlock();

  move(cursor_row, cursor_col);
}

void TerminalCollaborator::ProcessKey(AppEnv* app_env, int key) {
  absl::MutexLock lock(&mu_);

  Log() << "TerminalCollaborator::ProcessKey: " << key;

  auto left1 = [&]() {
    mu_.AssertHeld();
    String::Iterator it(state_.content, cursor_);
    cursor_row_ -= it.value() == '\n';
    it.MovePrev();
    cursor_ = it.id();
  };

  auto right1 = [&]() {
    mu_.AssertHeld();
    String::Iterator it(state_.content, cursor_);
    it.MoveNext();
    cursor_row_ += it.value() == '\n';
    cursor_ = it.id();
  };

  auto down1 = [&]() {
    mu_.AssertHeld();
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
  };

  auto up1 = [&]() {
    mu_.AssertHeld();
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
  };

  auto select_mode = [&](bool sel) {
    mu_.AssertHeld();
    if (!sel) {
      selection_anchor_ = ID();
    } else if (selection_anchor_ == ID()) {
      selection_anchor_ = cursor_;
    }
    Log() << "SM: " << sel << " --> " << absl::StrJoin(selection_anchor_, ":")
          << " " << absl::StrJoin(cursor_, ":") << " "
          << absl::StrJoin(ID(), ":");
  };

  auto delete_selection = [&]() {
    if (selection_anchor_ == ID()) return;
    state_.content.MakeRemove(&commands_, cursor_, selection_anchor_);
    String::Iterator it(state_.content, cursor_);
    it.MovePrev();
    cursor_ = it.id();
  };

  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);

  switch (key) {
    case KEY_SLEFT:
      select_mode(true);
      left1();
      used_();
      break;
    case KEY_LEFT:
      select_mode(false);
      left1();
      used_();
      break;
    case KEY_SRIGHT:
      select_mode(true);
      right1();
      used_();
      break;
    case KEY_RIGHT:
      select_mode(false);
      right1();
      used_();
      break;
    case KEY_HOME: {
      select_mode(false);
      String::Iterator it(state_.content, cursor_);
      while (!it.is_begin() && it.value() != '\n') it.MovePrev();
      cursor_ = it.id();
      used_();
    } break;
    case KEY_END: {
      select_mode(false);
      String::Iterator it(state_.content, cursor_);
      it.MoveNext();
      while (!it.is_end() && it.value() != '\n') it.MoveNext();
      it.MovePrev();
      cursor_ = it.id();
      used_();
    } break;
    case KEY_PPAGE:
      select_mode(false);
      for (int i = 0; i < fb_rows; i++) up1();
      used_();
      break;
    case KEY_NPAGE:
      select_mode(false);
      for (int i = 0; i < fb_rows; i++) down1();
      used_();
      break;
    case KEY_SR:  // shift down on my mac
      select_mode(true);
      up1();
      used_();
      break;
    case KEY_SF:
      select_mode(true);
      down1();
      used_();
      break;
    case KEY_UP:
      select_mode(false);
      up1();
      used_();
      break;
    case KEY_DOWN:
      select_mode(false);
      down1();
      used_();
      break;
    case 127:
    case KEY_BACKSPACE: {
      select_mode(false);
      state_.content.MakeRemove(&commands_, cursor_);
      String::Iterator it(state_.content, cursor_);
      it.MovePrev();
      cursor_ = it.id();
    } break;
    case ctrl('C'):
      if (selection_anchor_ != ID()) {
        app_env->clipboard = state_.content.Render(cursor_, selection_anchor_);
      }
      break;
    case ctrl('X'):
      if (selection_anchor_ != ID()) {
        app_env->clipboard = state_.content.Render(cursor_, selection_anchor_);
        delete_selection();
        select_mode(false);
      }
      break;
    case ctrl('V'):
      if (selection_anchor_ != ID()) {
        delete_selection();
        select_mode(false);
      }
      cursor_ = state_.content.MakeInsert(&commands_, site(),
                                          app_env->clipboard, cursor_);
      break;
    case 10: {
      delete_selection();
      select_mode(false);
      cursor_ = state_.content.MakeInsert(&commands_, site(), key, cursor_);
      cursor_row_++;
    } break;
    default: {
      if (key >= 32 && key < 127) {
        delete_selection();
        select_mode(false);
        cursor_ = state_.content.MakeInsert(&commands_, site(), key, cursor_);
      }
    } break;
  }
}
