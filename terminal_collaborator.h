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

class TerminalCollaborator final : public Collaborator {
 public:
  TerminalCollaborator(const Buffer* buffer, std::function<void()> invalidate);
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

  void Render(absl::Time last_key_press);
  void ProcessKey(int key);

 private:
  const std::function<void()> invalidate_;
  const std::function<void()> used_;
  absl::Mutex mu_;
  std::vector<String::CommandPtr> commands_ GUARDED_BY(mu_);
  bool recently_used_ GUARDED_BY(mu_);
  bool shutdown_ GUARDED_BY(mu_);
  EditNotification state_ GUARDED_BY(mu_);
  ID cursor_ GUARDED_BY(mu_);
  int cursor_row_ GUARDED_BY(mu_);
  SideBufferRef active_side_buffer_ GUARDED_BY(mu_);
};
