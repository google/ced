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
    : AsyncCollaborator("terminal", absl::Seconds(0), absl::Milliseconds(6)), buffer_(buffer),
      invalidate_(invalidate),
      editor_(site()),
      recently_used_(false),
      state_(State::EDITING) {}

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

  std::vector<std::string> profile = buffer_->ProfileData();
  side_bar.AddItem(LAY_TOP | LAY_HFILL, [this, profile](TerminalRenderContext* context) {
    int row = 0;
    for (const auto& s : profile) {
      context->Put(row++, 0, s, context->color->Theme(Tag(), 0));
    }
  }).FixSize(0, profile.size());

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

template <class EditorType>
static bool MaybeProcessEditingKey(AppEnv* app_env, int key,
                                   EditorType* editor) {
  int fb_rows, fb_cols;
  getmaxyx(stdscr, fb_rows, fb_cols);
  switch (key) {
    case KEY_SLEFT:
      editor->SelectLeft();
      break;
    case KEY_LEFT:
      editor->MoveLeft();
      break;
    case KEY_SRIGHT:
      editor->SelectRight();
      break;
    case KEY_RIGHT:
      editor->MoveRight();
      break;
    case KEY_HOME:
      editor->MoveStartOfLine();
      break;
    case KEY_END:
      editor->MoveEndOfLine();
      break;
    case KEY_PPAGE:
      editor->MoveUpN(fb_rows);
      break;
    case KEY_NPAGE:
      editor->MoveDownN(fb_rows);
      break;
    case KEY_SR:  // shift down on my mac
      editor->SelectUp();
      break;
    case KEY_SF:
      editor->SelectDown();
      break;
    case KEY_UP:
      editor->MoveUp();
      break;
    case KEY_DOWN:
      editor->MoveDown();
      break;
    case 127:
    case KEY_BACKSPACE:
      editor->Backspace();
      break;
    case ctrl('C'):
      editor->Copy(app_env);
      break;
    case ctrl('X'):
      editor->Cut(app_env);
      break;
    case ctrl('V'):
      editor->Paste(app_env);
      break;
    default:
      if (key >= 32 && key < 127) {
        editor->InsChar(key);
        break;
      }
      return false;
  }
  return true;
}

void TerminalCollaborator::ProcessKey(AppEnv* app_env, int key) {
  absl::MutexLock lock(&mu_);

  Log() << "TerminalCollaborator::ProcessKey: " << key;

  recently_used_ = true;
  switch (state_) {
    case State::EDITING:
      if (!MaybeProcessEditingKey(app_env, key, &editor_)) {
        switch (key) {
          case ctrl('F'):
            state_ = State::FINDING;
            break;
          case 10:
            editor_.InsNewLine();
            break;
        }
      }
      break;
    case State::FINDING:
      if (!MaybeProcessEditingKey(app_env, key, &find_editor_)) {
        switch (key) {
          case 10:
            state_ = State::EDITING;
            break;
        }
      }
      break;
  }
}
