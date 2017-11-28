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

#include <unordered_set>
#include "buffer.h"
#include "fswatch.h"
#include "log.h"

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
  std::unordered_set<std::string> last_ GUARDED_BY(mu_);
  bool update_ GUARDED_BY(mu_);
  bool shutdown_ GUARDED_BY(mu_);
  std::unique_ptr<FSWatcher> fswatch_ GUARDED_BY(mu_);
};

void ReferencedFileCollaborator::Push(const EditNotification& notification) {
  absl::MutexLock lock(&mu_);
  if (notification.shutdown) {
    shutdown_ = true;
  }
  std::unordered_set<std::string> referenced;
  notification.content.ForEachAttribute(
      Attribute::kDependency, [&](ID id, const Attribute& attr) {
        referenced.insert(attr.dependency().filename());
      });
  if (referenced != last_) {
    Log() << "CHANGED FILE SET";
    last_.swap(referenced);
    RestartWatch();
  }
}

EditResponse ReferencedFileCollaborator::Pull() {
  auto ready = [this]() {
    mu_.AssertHeld();
    return update_ || shutdown_;
  };
  EditResponse r;
  mu_.LockWhen(absl::Condition(&ready));
  r.referenced_file_changed = update_;
  update_ = false;
  r.done = shutdown_;
  mu_.Unlock();
  return r;
}

void ReferencedFileCollaborator::ChangedFile(bool shutdown_fswatch) {
  Log() << "REF:WATCHED CHANGED" << shutdown_fswatch;
  absl::MutexLock lock(&mu_);
  update_ = true;
  if (!shutdown_fswatch) {
    RestartWatch();
  }
}

void ReferencedFileCollaborator::RestartWatch() {
  std::vector<std::string> interest_vec;
  for (const auto& s : last_) {
    Log() << "INTEREST SET:" << s;
    interest_vec.push_back(s);
  }
  fswatch_.reset(new FSWatcher(
      interest_vec, [this](bool shutdown) { ChangedFile(shutdown); }));
}

SERVER_COLLABORATOR(ReferencedFileCollaborator, buffer) { return true; }
