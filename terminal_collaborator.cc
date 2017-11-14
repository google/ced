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

void TerminalCollaborator::Render(TerminalRenderContainers containers) {
  auto ready = [this]() {
    mu_.AssertHeld();
    return editor_.HasMostRecentEdit();
  };

  /*
   * edit item
   */
  containers.main
      .AddItem(LAY_LEFT | LAY_VFILL,
               [this, ready](TerminalRenderContext* context) {
                 mu_.LockWhen(absl::Condition(&ready));
                 editor_.Render(context);
                 mu_.Unlock();
               })
      .FixSize(80, 0);

  auto side_bar = containers.main.AddContainer(LAY_FILL, LAY_COLUMN);

  side_bar.AddItem(LAY_FILL, [this, ready](TerminalRenderContext* context) {
    mu_.LockWhen(absl::Condition(&ready));
    editor_.RenderSideBar(context);
    mu_.Unlock();
  });

  auto diagnostics =
      containers.ext_status.AddContainer(LAY_TOP | LAY_HFILL, LAY_COLUMN);

  absl::MutexLock lock(&mu_);
  editor_.CurrentState().diagnostics.ForEachValue(
      [&](const Diagnostic& diagnostic) {
        std::string msg = absl::StrCat(diagnostic.index, ": [",
                                       SeverityString(diagnostic.severity),
                                       "] ", diagnostic.message);
        diagnostics
            .AddItem(LAY_TOP | LAY_HFILL,
                     [msg](TerminalRenderContext* context) {
                       Log() << *context->window;
                       context->Put(0, 0, msg, context->color->Theme(Tag(), 0));
                     })
            .FixSize(0, 1);
      });
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
