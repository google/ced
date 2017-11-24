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
#include "buffer.h"
#include "absl/strings/str_cat.h"
#include "log.h"

namespace {

class CollaboratorRegistry {
 public:
  static CollaboratorRegistry& Get() {
    static CollaboratorRegistry r;
    return r;
  }

  void Register(std::function<void(Buffer*)> f) { collabs_.push_back(f); }

  void Run(Buffer* buffer) {
    for (auto f : collabs_) {
      f(buffer);
    }
  }

 private:
  std::vector<std::function<void(Buffer*)> > collabs_;
};
}

Buffer::Buffer(const std::string& filename, absl::optional<AnnotatedString> initial_string)
    : synthetic_(initial_string), version_(0),
      updating_(false),
      last_used_(absl::Now() - absl::Seconds(1000000)),
      filename_(filename) {
  if (initial_string) state_.content = *initial_string;
  init_thread_ = std::thread([this]() {
    CollaboratorRegistry::Get().Run(this);
  });
}

void Buffer::RegisterCollaborator(
    std::function<void(Buffer*)> maybe_init_collaborator) {
  CollaboratorRegistry::Get().Register(maybe_init_collaborator);
}

Buffer::~Buffer() {
  init_thread_.join();

  UpdateState(nullptr, false,
              [](EditNotification& state) { state.shutdown = true; });

  for (auto& t : collaborator_threads_) {
    t.join();
  }
}

void Buffer::AddCollaborator(AsyncCollaboratorPtr&& collaborator) {
  absl::MutexLock lock(&mu_);
  AsyncCollaborator* raw = collaborator.get();
  collaborators_.emplace_back(std::move(collaborator));
  collaborator_threads_.emplace_back([this, raw]() {
    try {
      RunPull(raw);
    } catch (std::exception& e) {
      Log() << raw->name() << " collaborator pull broke: " << e.what();
    }
    absl::MutexLock lock(&mu_);
    done_collaborators_.insert(raw);
  });
  collaborator_threads_.emplace_back([this, raw]() {
    try {
      RunPush(raw);
    } catch (std::exception& e) {
      Log() << raw->name() << " collaborator push broke: " << e.what();
    }
  });
}

void Buffer::AddCollaborator(SyncCollaboratorPtr&& collaborator) {
  absl::MutexLock lock(&mu_);
  SyncCollaborator* raw = collaborator.get();
  collaborators_.emplace_back(std::move(collaborator));
  collaborator_threads_.emplace_back([this, raw]() {
    try {
      RunSync(raw);
    } catch (std::exception& e) {
      Log() << raw->name() << " collaborator sync broke: " << e.what();
    }
    absl::MutexLock lock(&mu_);
    done_collaborators_.insert(raw);
  });
}

namespace {
struct Shutdown {};
}  // namespace

EditNotification Buffer::NextNotification(Collaborator* collaborator,
                                          uint64_t* last_processed) {
  auto all_edits_complete = [this]() {
    mu_.AssertHeld();
    return state_.shutdown &&
           declared_no_edit_collaborators_.size() == collaborators_.size();
  };
  auto processable = [&]() {
    mu_.AssertHeld();
    return version_ != *last_processed || all_edits_complete();
  };
  // wait until something interesting to work on
  mu_.LockWhen(absl::Condition(&processable));
  if (version_ != *last_processed) {
    absl::Time first_saw_change = absl::Now();
    if (!state_.shutdown) {
      absl::Time last_used_at_start;
      do {
        Log() << collaborator->name() << " last_used: " << last_used_;
        last_used_at_start = last_used_;
        absl::Duration idle_time = absl::Now() - last_used_;
        absl::Duration time_from_change = absl::Now() - first_saw_change;
        Log() << collaborator->name() << " idle_time: " << idle_time
              << " time_from_change: " << time_from_change;
        if (*last_processed != 0 &&
            mu_.AwaitWithTimeout(
                absl::Condition(&state_.shutdown),
                std::max(collaborator->push_delay_from_idle() - idle_time,
                         collaborator->push_delay_from_start() -
                             time_from_change))) {
          break;
        }
      } while (last_used_ != last_used_at_start && !state_.shutdown);
    }
    *last_processed = version_;
    EditNotification notification = state_;
    collaborator->MarkRequest();
    mu_.Unlock();
    Log() << collaborator->name() << " notify";
    return notification;
  } else {
    assert(all_edits_complete());
    done_collaborators_.insert(collaborator);
    mu_.Unlock();
    throw Shutdown();
  }
}

static bool HasUpdates(const EditResponse& response) {
  return response.become_loaded || response.referenced_file_changed ||
         !response.content_updates.commands().empty();
}

void IntegrateResponse(const EditResponse& response, EditNotification* state) {
  state->content = state->content.Integrate(response.content_updates);
  if (response.become_loaded) state->fully_loaded = true;
  if (response.referenced_file_changed) state->referenced_file_version++;
}

void Buffer::UpdateState(Collaborator* collaborator, bool become_used,
                         std::function<void(EditNotification& state)> f) {
  auto updatable = [this]() {
    mu_.AssertHeld();
    return !updating_;
  };

  // get the update lock
  mu_.LockWhen(absl::Condition(&updatable));
  if (collaborator) collaborator->MarkChange();
  updating_ = true;
  auto state = state_;
  mu_.Unlock();

  f(state);

  // commit the update and advance time
  mu_.Lock();
  updating_ = false;
  version_++;
  declared_no_edit_collaborators_ = done_collaborators_;
  state_ = state;
  if (become_used) {
    last_used_ = absl::Now();
  }
  mu_.Unlock();
}

void Buffer::SinkResponse(Collaborator* collaborator,
                          const EditResponse& response) {
  {
    absl::MutexLock lock(&mu_);
    collaborator->MarkResponse();
  }

  if (HasUpdates(response)) {
    UpdateState(collaborator, response.become_used,
                [&](EditNotification& state) {
                  Log() << collaborator->name() << " integrating";
                  IntegrateResponse(response, &state);
                });
  } else {
    Log() << collaborator->name() << " gives an empty update";
    absl::MutexLock lock(&mu_);
    if (response.become_used) {
      last_used_ = absl::Now();
    }
    declared_no_edit_collaborators_.insert(collaborator);
  }

  if (response.done) {
    absl::MutexLock lock(&mu_);
    done_collaborators_.insert(collaborator);
    throw Shutdown();
  }
}

void Buffer::RunPush(AsyncCollaborator* collaborator) {
  uint64_t processed_version = 0;
  try {
    for (;;) {
      collaborator->Push(NextNotification(collaborator, &processed_version));
    }
  } catch (Shutdown) {
    return;
  }
}

void Buffer::RunPull(AsyncCollaborator* collaborator) {
  try {
    for (;;) {
      SinkResponse(collaborator, collaborator->Pull());
    }
  } catch (Shutdown) {
    return;
  }
}

void Buffer::RunSync(SyncCollaborator* collaborator) {
  uint64_t processed_version = 0;
  try {
    for (;;) {
      SinkResponse(collaborator, collaborator->Edit(NextNotification(
                                     collaborator, &processed_version)));
    }
  } catch (Shutdown) {
    return;
  }
}

std::vector<std::string> Buffer::ProfileData() const {
  auto now = absl::Now();
  absl::MutexLock lock(&mu_);
  std::vector<std::string> out;
  for (const auto& c : collaborators_) {
  auto report = [&out, now, this, &c](const char* name, absl::Time timestamp) {
    auto age = now - timestamp;
    if (age > absl::Seconds(5)) return;
    out.emplace_back(absl::StrCat(filename(), ":", c->name(), ":", name, ": ", absl::FormatTime(timestamp), " (", absl::FormatDuration(age), " ago)"));
  };
    report("chg", c->last_change());
    report("rsp", c->last_response());
    report("rqst", c->last_request());
  }
  return out;
}
