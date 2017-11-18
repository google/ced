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

#include "buffer.h"
#include "fswatch.h"

class ReferencedFileCollaborator final : public AsyncCollaborator {
 public:
  ReferencedFileCollaborator(const Buffer* buffer)
      : AsyncCollaborator("reffile", absl::Seconds(0),
                          absl::Milliseconds(100)) {}
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;

 private:
  void ChangedFile(bool shutdown_fswatch);
  void RestartWatch() EXCLUSIVE_LOCKS_REQUIRED(mu_);

  absl::Mutex mu_;
  USet<std::string> last_ GUARDED_BY(mu_);
  bool update_ GUARDED_BY(mu_);
  bool shutdown_ GUARDED_BY(mu_);
  std::unique_ptr<FSWatcher> fswatch_ GUARDED_BY(mu_);
};
