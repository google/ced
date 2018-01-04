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
#include "client_collaborator.h"
#include <gflags/gflags.h>
#include <deque>
#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "log.h"

DEFINE_bool(buffer_profile_display, false,
            "Show buffer collaborator latency HUD");

DEFINE_bool(editor_debug_display, false, "Show editor debug information");

absl::Mutex ClientCollaborator::mu_;
std::vector<ClientCollaborator*> ClientCollaborator::all_;

absl::Mutex Invalidator::mu_;
std::vector<Invalidator*> Invalidator::invalidators_;

Invalidator::Invalidator() {
  absl::MutexLock lock(&mu_);
  invalidators_.push_back(this);
}

Invalidator::~Invalidator() {
  absl::MutexLock lock(&mu_);
  invalidators_.erase(
      std::remove(invalidators_.begin(), invalidators_.end(), this),
      invalidators_.end());
}

void Invalidator::InvalidateAll() {
  absl::MutexLock lock(&mu_);
  for (auto p : invalidators_) p->Invalidate();
}

ClientCollaborator::ClientCollaborator(const Buffer* buffer)
    : AsyncCollaborator("terminal", absl::Seconds(0), absl::Seconds(0)),
      buffer_(buffer),
      editor_(Editor::Make(buffer_->site(), buffer_->filename().string(),
                           !buffer->synthetic())),
      recently_used_(false) {
  absl::MutexLock lock(&mu_);
  all_.push_back(this);
}

ClientCollaborator::~ClientCollaborator() {
  absl::MutexLock lock(&mu_);
  all_.erase(std::remove(all_.begin(), all_.end(), this), all_.end());
}

void ClientCollaborator::All_Render(RenderContainers containers, Theme* theme) {
  absl::MutexLock lock(&mu_);
  for (auto t : all_) t->Render(containers, theme);
}

void ClientCollaborator::Push(const EditNotification& notification) {
  LogTimer tmr("term_push");
  {
    absl::MutexLock lock(&mu_);
    tmr.Mark("lock");
    editor_->UpdateState(&tmr, notification);
    tmr.Mark("update");
  }
  Invalidator::InvalidateAll();
}

EditResponse ClientCollaborator::Pull() {
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

  Invalidator::InvalidateAll();

  return r;
}

void ClientCollaborator::Render(RenderContainers containers, Theme* theme) {
  /*
   * edit item
   */
  editor_->Render(theme,
                  buffer_->synthetic() ? containers.side_bar : containers.main);

  if (FLAGS_buffer_profile_display) {
    containers.side_bar->MakeSimpleText(theme->ThemeToken({}, 0),
                                        buffer_->ProfileData());
  }

  if (FLAGS_editor_debug_display) {
    containers.side_bar->MakeSimpleText(theme->ThemeToken({}, 0),
                                        editor_->DebugData());
  }

#if 0
  side_bar.AddItem(LAY_FILL, [this](TerminalRenderContext* context) {
    absl::MutexLock lock(&mu_);
    editor_->RenderSideBar(context);
  });
#endif

#if 0
  auto diagnostics = containers.ext_status->MakeColumn(
      absl::StrCat("diagnostics:", buffer_->filename().string()));

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
#endif
}

CLIENT_COLLABORATOR(ClientCollaborator, buffer) { return true; }
