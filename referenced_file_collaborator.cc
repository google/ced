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

#include "referenced_file_collaborator.h"
#include <unordered_set>

void ReferencedFileCollaborator::Push(const EditNotification& notification) {
  absl::MutexLock lock(&mu_);
  if (notification.shutdown) {
    shutdown_ = true;
  }
  if (!last_.SameIdentity(notification.referenced_files)) {
  Log() << "CHANGED FILE SET";
    last_ = notification.referenced_files;
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
  std::unordered_set<std::string> interest_set;
  last_.ForEachValue(
      [&interest_set](const std::string& s) { interest_set.insert(s); });
  std::vector<std::string> interest_vec;
  for (const auto& s : interest_set) {
    Log() << "INTEREST SET:" << s;
    interest_vec.push_back(s);
  }
  fswatch_.reset(new FSWatcher(
      interest_vec, [this](bool shutdown) { ChangedFile(shutdown); }));
}
