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
      editor_(site()),
      recently_used_(false) {}

void TerminalCollaborator::Push(const EditNotification& notification) {
  {
    absl::MutexLock lock(&mu_);
    editor_.UpdateState(notification);
  }
  invalidate_();
}

EditResponse TerminalCollaborator::Pull() {
  auto ready = [this]() {
    mu_.AssertHeld();
    return editor_.HasCommands() || recently_used_;
  };

  mu_.LockWhen(absl::Condition(&ready));
  EditResponse r = editor_.MakeResponse();
  r.become_used |= recently_used_;
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

void TerminalCollaborator::Render(TerminalRenderer::ContainerRef container) {
  /*
   * edit item
   */
  container
      .AddItem(LAY_FILL,
               [this](TerminalRenderContext* context) {
                 auto ready = [this]() {
                   mu_.AssertHeld();
                   return editor_.HasMostRecentEdit();
                 };

                 mu_.LockWhen(absl::Condition(&ready));

                 editor_.Render(context);

                 mu_.Unlock();
               })
      .FixSize(80, 0);

#if 0
  container.AddItem(LAY_FILL, [this](TerminalRenderContext* context) {
    absl::MutexLock lock(&mu_);

    auto r = *context->window;

    int max_length = r.width();
    int row = 0;
    state_.diagnostics.ForEachValue([&](const Diagnostic& diagnostic) {
      if (row >= r.height()) return;
      std::string msg = absl::StrCat(diagnostic.index, ": [",
                                     SeverityString(diagnostic.severity), "] ",
                                     diagnostic.message);
      if (msg.length() > max_length) {
        msg.resize(max_length);
      }
      context->Put(row++, 0, msg, context->color->Theme(Tag(), 0));
    });

    cursor_token_.ForEach([&](const std::string& msg) {
      if (row >= r.height()) return;
      context->Put(row++, 0, msg, context->color->Theme(Tag(), 0));
    });

    state_.side_buffers.ForEachValue(
        active_side_buffer_.name, [&](const SideBuffer& buffer) {
          mu_.AssertHeld();
          if (row >= r.height()) return;
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
            if (active_side_buffer_.lines.back() >=
                sb_cursor_row_ + r.height()) {
              sb_cursor_row_ = active_side_buffer_.lines.back() - r.height();
              adj++;
            }
            if (adj == 2) {
              sb_cursor_row_ =
                  (buffer.line_ofs.front() + buffer.line_ofs.back()) / 2 -
                  r.height() / 2;
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
            auto attr = context->color->Theme(Tag(), flags);
            char c = buffer.content[i];
            if (c == '\n') {
              chtype fill = ' ';
              while (col < r.width()) {
                context->Put(row, col++, fill, attr);
              }
              row++;
              sbrow++;
              col = 0;
              if (row >= r.height()) break;
            } else if (col < r.width()) {
              context->Put(row, col++, c, attr);
            }
          }
        });
  });

  move(cursor_row, cursor_col);
#endif
}

void TerminalCollaborator::ProcessKey(AppEnv* app_env, int key) {
  absl::MutexLock lock(&mu_);

  Log() << "TerminalCollaborator::ProcessKey: " << key;

  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);

  recently_used_ = true;
  switch (key) {
    case KEY_SLEFT:
      editor_.SelectLeft();
      break;
    case KEY_LEFT:
      editor_.MoveLeft();
      break;
    case KEY_SRIGHT:
      editor_.SelectRight();
      break;
    case KEY_RIGHT:
      editor_.MoveRight();
      break;
    case KEY_HOME:
      editor_.MoveStartOfLine();
      break;
    case KEY_END:
      editor_.MoveEndOfLine();
      break;
    case KEY_PPAGE:
      editor_.MoveUpN(fb_rows);
      break;
    case KEY_NPAGE:
      editor_.MoveDownN(fb_rows);
      break;
    case KEY_SR:  // shift down on my mac
      editor_.SelectUp();
      break;
    case KEY_SF:
      editor_.SelectDown();
      break;
    case KEY_UP:
      editor_.MoveUp();
      break;
    case KEY_DOWN:
      editor_.MoveDown();
      break;
    case 127:
    case KEY_BACKSPACE:
      editor_.Backspace();
      break;
    case ctrl('C'):
      editor_.Copy(app_env);
      break;
    case ctrl('X'):
      editor_.Cut(app_env);
      break;
    case ctrl('V'):
      editor_.Paste(app_env);
      break;
    case 10:
      editor_.InsNewLine();
      break;
    default: {
      if (key >= 32 && key < 127) {
        editor_.InsChar(key);
      }
    } break;
  }
}
