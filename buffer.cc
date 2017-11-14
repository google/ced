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
#include "io_collaborator.h"
#include "log.h"

Buffer::Buffer(const std::string& filename)
    : version_(0),
      updating_(false),
      last_used_(absl::Now() - absl::Seconds(1000000)),
      filename_(filename) {
  MakeCollaborator<IOCollaborator>();
}

Buffer::~Buffer() {
  UpdateState(false, [](EditNotification& state) { state.shutdown = true; });

  for (auto& t : collaborator_threads_) {
    t.join();
  }
}

void Buffer::AddCollaborator(CollaboratorPtr&& collaborator) {
  absl::MutexLock lock(&mu_);
  Collaborator* raw = collaborator.get();
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
  sync_collaborators_.emplace_back(std::move(collaborator));
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

EditNotification Buffer::NextNotification(void* id, const char* name,
                                          uint64_t* last_processed,
                                          absl::Duration push_delay) {
  auto all_edits_complete = [this]() {
    mu_.AssertHeld();
    return state_.shutdown &&
           declared_no_edit_collaborators_.size() ==
               collaborators_.size() + sync_collaborators_.size();
  };
  auto processable = [&]() {
    mu_.AssertHeld();
    return version_ != *last_processed || all_edits_complete();
  };
  // wait until something interesting to work on
  mu_.LockWhen(absl::Condition(&processable));
  if (version_ != *last_processed) {
    if (!state_.shutdown) {
      absl::Time last_used_at_start;
      do {
        Log() << name << " last_used: " << last_used_;
        last_used_at_start = last_used_;
        absl::Duration idle_time = absl::Now() - last_used_;
        Log() << name << " idle_time: " << idle_time;
        if (*last_processed != 0 &&
            mu_.AwaitWithTimeout(absl::Condition(&state_.shutdown),
                                 push_delay - idle_time)) {
          break;
        }
      } while (last_used_ != last_used_at_start);
    }
    *last_processed = version_;
    EditNotification notification = state_;
    mu_.Unlock();
    Log() << name << " notify";
    return notification;
  } else {
    assert(all_edits_complete());
    done_collaborators_.insert(id);
    mu_.Unlock();
    throw Shutdown();
  }
}

static bool HasUpdates(const EditResponse& response) {
  return response.become_loaded || !response.content.empty() ||
         !response.token_types.empty() || !response.diagnostics.empty() ||
         !response.diagnostic_ranges.empty() ||
         !response.side_buffers.empty() || !response.side_buffer_refs.empty() ||
         !response.fixits.empty();
}

template <class T>
static void IntegrateState(T* state, const typename T::CommandBuf& commands) {
  for (const auto& cmd : commands) {
    *state = state->Integrate(cmd);
  }
}

void Buffer::UpdateState(bool become_used,
                         std::function<void(EditNotification& state)> f) {
  auto updatable = [this]() {
    mu_.AssertHeld();
    return !updating_;
  };

  // get the update lock
  mu_.LockWhen(absl::Condition(&updatable));
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

void Buffer::SinkResponse(void* id, const char* name,
                          const EditResponse& response) {
  if (HasUpdates(response)) {
    UpdateState(response.become_used, [&](EditNotification& state) {
      Log() << name << " integrating";
      IntegrateState(&state.content, response.content);
      IntegrateState(&state.token_types, response.token_types);
      IntegrateState(&state.diagnostics, response.diagnostics);
      IntegrateState(&state.diagnostic_ranges, response.diagnostic_ranges);
      IntegrateState(&state.side_buffers, response.side_buffers);
      IntegrateState(&state.side_buffer_refs, response.side_buffer_refs);
      IntegrateState(&state.fixits, response.fixits);
      if (response.become_loaded) state.fully_loaded = true;
    });
  } else {
    Log() << name << " gives an empty update";
    absl::MutexLock lock(&mu_);
    if (response.become_used) {
      last_used_ = absl::Now();
    }
    declared_no_edit_collaborators_.insert(id);
  }

  if (response.done) {
    absl::MutexLock lock(&mu_);
    done_collaborators_.insert(id);
    throw Shutdown();
  }
}

void Buffer::RunPush(Collaborator* collaborator) {
  uint64_t processed_version = 0;
  try {
    for (;;) {
      collaborator->Push(NextNotification(collaborator, collaborator->name(),
                                          &processed_version,
                                          collaborator->push_delay()));
    }
  } catch (Shutdown) {
    return;
  }
}

void Buffer::RunPull(Collaborator* collaborator) {
  try {
    for (;;) {
      SinkResponse(collaborator, collaborator->name(), collaborator->Pull());
    }
  } catch (Shutdown) {
    return;
  }
}

void Buffer::RunSync(SyncCollaborator* collaborator) {
  uint64_t processed_version = 0;
  try {
    for (;;) {
      SinkResponse(collaborator, collaborator->name(),
                   collaborator->Edit(NextNotification(
                       collaborator, collaborator->name(), &processed_version,
                       collaborator->push_delay())));
    }
  } catch (Shutdown) {
    return;
  }
}
