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
#pragma once

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "buffer.h"
#include "editor.h"
#include "line_editor.h"
#include "log.h"
#include "render.h"
#include "terminal_color.h"

struct TerminalRenderContainers {
  Widget* main;
  Widget* side_bar;
};

extern void InvalidateTerminal();

class TerminalCollaborator final : public AsyncCollaborator {
 public:
  TerminalCollaborator(const Buffer* buffer);
  ~TerminalCollaborator();
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;

  static void All_Render(TerminalRenderContainers containers, Theme* theme);

 private:
  void Render(TerminalRenderContainers containers, Theme* theme)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const Buffer* const buffer_;
  static absl::Mutex mu_;
  std::shared_ptr<Editor> editor_ GUARDED_BY(mu_);
  LineEditor find_editor_ GUARDED_BY(mu_);
  bool recently_used_ GUARDED_BY(mu_);

  static std::vector<TerminalCollaborator*> all_ GUARDED_BY(mu_);
};
