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
#include <gflags/gflags.h>
#include <deque>
#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "log.h"

DEFINE_bool(buffer_profile_display, false,
            "Show buffer collaborator latency HUD");

absl::Mutex TerminalCollaborator::mu_;
std::vector<TerminalCollaborator*> TerminalCollaborator::all_;

constexpr char ctrl(char c) { return c & 0x1f; }

TerminalCollaborator::TerminalCollaborator(const Buffer* buffer)
    : AsyncCollaborator("terminal", absl::Seconds(0), absl::Seconds(0)),
      buffer_(buffer),
      editor_(Editor::Make(site())),
      recently_used_(false),
      state_(State::EDITING) {
  absl::MutexLock lock(&mu_);
  all_.push_back(this);
}

TerminalCollaborator::~TerminalCollaborator() {
  absl::MutexLock lock(&mu_);
  all_.erase(std::remove(all_.begin(), all_.end(), this), all_.end());
}

void TerminalCollaborator::All_Render(TerminalRenderContainers containers) {
  absl::MutexLock lock(&mu_);
  for (auto t : all_) t->Render(containers);
}

void TerminalCollaborator::All_ProcessKey(AppEnv* app_env, int key) {
  absl::MutexLock lock(&mu_);
  for (auto t : all_) t->ProcessKey(app_env, key);
}

void TerminalCollaborator::Push(const EditNotification& notification) {
  LogTimer tmr("term_push");
  {
    absl::MutexLock lock(&mu_);
    tmr.Mark("lock");
    editor_->UpdateState(&tmr, notification);
    tmr.Mark("update");
  }
  InvalidateTerminal();
}

EditResponse TerminalCollaborator::Pull() {
  auto ready = [this]() {
    mu_.AssertHeld();
    return editor_->HasCommands() || recently_used_;
  };

  mu_.LockWhen(absl::Condition(&ready));
  LogTimer tmr("term_pull");
  EditResponse r = editor_->MakeResponse();
  tmr.Mark("make");
  r.become_used |= recently_used_;
  recently_used_ = false;
  mu_.Unlock();
  tmr.Mark("unlock");

  return r;
}

void TerminalCollaborator::Render(TerminalRenderContainers containers) {
  /*
   * edit item
   */
  (buffer_->synthetic() ? containers.side_bar : containers.main)
      .AddItem(
          LAY_LEFT | LAY_VFILL,
          editor_->PrepareRender<TerminalRenderContext>(!buffer_->synthetic()))
      .FixSize(80, 0);

  if (FLAGS_buffer_profile_display) {
    std::vector<std::string> profile = buffer_->ProfileData();
    containers.side_bar
        .AddItem(LAY_TOP | LAY_HFILL,
                 [profile](TerminalRenderContext* context) {
                   context->animating = true;
                   int row = 0;
                   for (const auto& s : profile) {
                     context->Put(row++, 0, s, context->color->Theme({}, 0));
                   }
                 })
        .FixSize(0, profile.size());
  }

#if 0
  side_bar.AddItem(LAY_FILL, [this](TerminalRenderContext* context) {
    absl::MutexLock lock(&mu_);
    editor_->RenderSideBar(context);
  });
#endif

  auto diagnostics =
      containers.ext_status.AddContainer(LAY_TOP | LAY_HFILL, LAY_COLUMN);

  int num_diagnostics = 0;
  editor_->CurrentState().content.ForEachAttribute(
      Attribute::kDiagnostic, [&](ID, const Attribute& attribute) {
        if (num_diagnostics < 10) {
          std::string msg = absl::StrCat(
              "[", Diagnostic_Severity_Name(attribute.diagnostic().severity()),
              "] ", attribute.diagnostic().message());
          diagnostics
              .AddItem(LAY_TOP | LAY_HFILL,
                       [msg](TerminalRenderContext* context) {
                         Log() << *context->window;
                         context->Put(0, 0, msg, context->color->Theme({}, 0));
                       })
              .FixSize(0, 1);
          num_diagnostics++;
        }
      });
}

template <class EditorType>
static bool MaybeProcessEditingKey(AppEnv* app_env, int key,
                                   std::shared_ptr<EditorType> editor) {
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
  if (buffer_->synthetic()) return;

  LogTimer tmr("term_proc_key");
  tmr.Mark("locked");

  Log() << "TerminalCollaborator::ProcessKey: " << key;

  recently_used_ = true;
  switch (state_) {
    case State::EDITING:
      if (!MaybeProcessEditingKey(app_env, key, editor_)) {
        switch (key) {
          case ctrl('F'):
            // state_ = State::FINDING;
            break;
          case 10:
            editor_->InsNewLine();
            break;
        }
      }
      break;
#if 0
    case State::FINDING:
      if (!MaybeProcessEditingKey(app_env, key, &find_editor_)) {
        switch (key) {
          case 10:
            state_ = State::EDITING;
            break;
        }
      }
      break;
#endif
  }
}

IMPL_COLLABORATOR(TerminalCollaborator, buffer) { return true; }