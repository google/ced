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
#include "colors.h"
#include "log.h"

TerminalCollaborator::TerminalCollaborator(const Buffer* buffer,
                                           std::function<void()> invalidate)
    : Collaborator("terminal", absl::Seconds(0)),
      invalidate_(invalidate),
      used_([this]() {
        mu_.AssertHeld();
        recently_used_ = true;
      }),
      recently_used_(false),
      shutdown_(false),
      cursor_(String::Begin()),
      cursor_row_(0) {}

void TerminalCollaborator::Shutdown() {
  absl::MutexLock lock(&mu_);
  shutdown_ = true;
}

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
    return shutdown_ || !commands_.empty() || recently_used_;
  };

  mu_.LockWhen(absl::Condition(&ready));
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

void TerminalCollaborator::Render(absl::Time last_key_press) {
  auto ready = [this]() {
    mu_.AssertHeld();
    return shutdown_ || state_.content.Has(cursor_);
  };

  mu_.LockWhen(absl::Condition(&ready));

  if (shutdown_) {
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

  String::AllIterator it(state_.content, line_it.id());
  AnnotationTracker<Token> t_token(state_.token_types);
  AnnotationTracker<ID> t_diagnostic(state_.diagnostic_ranges);
  AnnotationTracker<SideBufferRef> t_side_buffer_ref(state_.side_buffer_refs);
  for (int row = 0; row < fb_rows; row++) {
    int col = 0;
    auto move_next = [&]() {
      if (it.id() == cursor_) {
        cursor_row = row;
        cursor_col = col;
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
    for (;;) {
      if (it.is_end()) {
        row = fb_rows;
        break;
      }
      if (it.is_visible()) {
        if (it.value() == '\n') break;
        chtype attr = 0;
        switch (t_token.cur()) {
          case Token::UNSET:
            attr = COLOR_PAIR(ColorID::DEFAULT);
            break;
          case Token::IDENT:
            attr = COLOR_PAIR(ColorID::IDENT);
            break;
          case Token::KEYWORD:
            attr = COLOR_PAIR(ColorID::KEYWORD);
            break;
          case Token::SYMBOL:
            attr = COLOR_PAIR(ColorID::SYMBOL);
            break;
          case Token::LITERAL:
            attr = COLOR_PAIR(ColorID::LITERAL);
            break;
          case Token::COMMENT:
            attr = COLOR_PAIR(ColorID::COMMENT);
            break;
        }
        if (t_diagnostic.cur() != ID()) {
          attr = COLOR_PAIR(ColorID::ERROR);
        }
        if (col < 80) {
          mvaddch(row, col, it.value() | attr);
        }
        col++;
      }
      move_next();
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

    state_.side_buffers.ForEachValue(
        active_side_buffer_.name, [&](const SideBuffer& buffer) {
          if (row >= fb_rows) return;
          int col = 0;
          int sbrow = 0;
          for (auto c : buffer.content) {
            if (c == '\n') {
              row++;
              sbrow++;
              col = 0;
              if (row >= fb_rows) break;
            } else if (col < fb_cols) {
              chtype attr = COLOR_PAIR(ColorID::DEFAULT);
              if (std::find(active_side_buffer_.lines.begin(),
                            active_side_buffer_.lines.end(),
                            sbrow) != active_side_buffer_.lines.end()) {
                attr = COLOR_PAIR(ColorID::KEYWORD);
              }
              mvaddch(row, 82 + (col++), c | attr);
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

void TerminalCollaborator::ProcessKey(int key) {
  absl::MutexLock lock(&mu_);

  Log() << "TerminalCollaborator::ProcessKey: " << key;

  auto down1 = [&]() {
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
    used_();
  };

  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);

  switch (key) {
    case KEY_LEFT: {
      String::Iterator it(state_.content, cursor_);
      cursor_row_ -= it.value() == '\n';
      it.MovePrev();
      cursor_ = it.id();
      used_();
    } break;
    case KEY_RIGHT: {
      String::Iterator it(state_.content, cursor_);
      it.MoveNext();
      cursor_row_ += it.value() == '\n';
      cursor_ = it.id();
      used_();
    } break;
    case KEY_HOME: {
      String::Iterator it(state_.content, cursor_);
      while (!it.is_begin() && it.value() != '\n') it.MovePrev();
      cursor_ = it.id();
      used_();
    } break;
    case KEY_END: {
      String::Iterator it(state_.content, cursor_);
      it.MoveNext();
      while (!it.is_end() && it.value() != '\n') it.MoveNext();
      it.MovePrev();
      cursor_ = it.id();
      used_();
    } break;
    case KEY_PPAGE:
      for (int i = 0; i < fb_rows; i++) up1();
      used_();
      break;
    case KEY_UP:
      up1();
      used_();
      break;
    case KEY_NPAGE:
      for (int i = 0; i < fb_rows; i++) down1();
      used_();
      break;
    case KEY_DOWN:
      down1();
      used_();
      break;
    case 127:
    case KEY_BACKSPACE: {
      state_.content.MakeRemove(&commands_, cursor_);
      String::Iterator it(state_.content, cursor_);
      it.MovePrev();
      cursor_ = it.id();
    } break;
    case 10: {
      cursor_ = state_.content.MakeInsert(&commands_, site(), key, cursor_);
      cursor_row_++;
    } break;
    default: {
      if (key >= 32 && key < 127) {
        cursor_ = state_.content.MakeInsert(&commands_, site(), key, cursor_);
      }
    } break;
  }
}
