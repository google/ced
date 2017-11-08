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
    : shutdown_(false),
      version_(0),
      updating_(false),
      last_used_(absl::Now() - absl::Seconds(1000000)),
      filename_(filename) {
  MakeCollaborator<IOCollaborator>();
}

Buffer::~Buffer() {
  for (auto& c : collaborators_) {
    c->Shutdown();
  }

  mu_.Lock();
  shutdown_ = true;
  mu_.Unlock();

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
  });
}

EditNotification Buffer::NextNotification(const char* name,
                                          uint64_t* last_processed,
                                          absl::Duration push_delay) {
  auto processable = [this, last_processed]() {
    mu_.AssertHeld();
    return shutdown_ || version_ != *last_processed;
  };
  // wait until something interesting to work on
  mu_.LockWhen(absl::Condition(&processable));
  absl::Time last_used_at_start;
  do {
    Log() << name << " last_used: " << last_used_;
    last_used_at_start = last_used_;
    absl::Duration idle_time = absl::Now() - last_used_;
    Log() << name << " idle_time: " << idle_time;
    if (*last_processed != 0 &&
        mu_.AwaitWithTimeout(absl::Condition(&shutdown_),
                             push_delay - idle_time)) {
      mu_.Unlock();
      throw std::runtime_error("Shutdown");
    }
  } while (last_used_ != last_used_at_start);
  *last_processed = version_;
  EditNotification notification = state_;
  mu_.Unlock();
  Log() << name << " notify";
  return notification;
}

static bool HasUpdates(const EditResponse& response) {
  return response.become_loaded || !response.content.empty() ||
         !response.token_types.empty() || !response.diagnostics.empty() ||
         !response.diagnostic_ranges.empty() ||
         !response.side_buffers.empty() || !response.side_buffer_refs.empty();
}

template <class T>
static void IntegrateState(T* state, const typename T::CommandBuf& commands) {
  for (const auto& cmd : commands) {
    *state = state->Integrate(cmd);
  }
}

void Buffer::SinkResponse(const char* name, const EditResponse& response) {
  auto updatable = [this]() {
    mu_.AssertHeld();
    return shutdown_ || !updating_;
  };
  if (HasUpdates(response)) {
    // get the update lock
    mu_.LockWhen(absl::Condition(&updatable));
    if (shutdown_) {
      mu_.Unlock();
      throw std::runtime_error("Done [[shutdown]]");
    }
    updating_ = true;
    auto state = state_;
    mu_.Unlock();

    Log() << name << " integrating";

    IntegrateState(&state.content, response.content);
    IntegrateState(&state.token_types, response.token_types);
    IntegrateState(&state.diagnostics, response.diagnostics);
    IntegrateState(&state.diagnostic_ranges, response.diagnostic_ranges);
    IntegrateState(&state.side_buffers, response.side_buffers);
    IntegrateState(&state.side_buffer_refs, response.side_buffer_refs);

    // commit the update and advance time
    mu_.Lock();
    updating_ = false;
    version_++;
    Log() << name << " version bump to " << version_;
    state_ = state;
    if (response.become_used) {
      last_used_ = absl::Now();
    }
    if (response.become_loaded) {
      state_.fully_loaded = true;
    }
    mu_.Unlock();
  } else {
    Log() << name << " gives an empty update";
    absl::MutexLock lock(&mu_);
    if (response.become_used) {
      last_used_ = absl::Now();
    }
    if (shutdown_) {
      throw std::runtime_error("Done [[shutdown]]");
    }
  }

  if (response.done) {
    throw std::runtime_error("Done [[response.done]]");
  }
}

void Buffer::RunPush(Collaborator* collaborator) {
  uint64_t processed_version = 0;
  for (;;) {
    collaborator->Push(NextNotification(
        collaborator->name(), &processed_version, collaborator->push_delay()));
  }
}

void Buffer::RunPull(Collaborator* collaborator) {
  for (;;) {
    SinkResponse(collaborator->name(), collaborator->Pull());
  }
}

void Buffer::RunSync(SyncCollaborator* collaborator) {
  uint64_t processed_version = 0;
  for (;;) {
    SinkResponse(collaborator->name(),
                 collaborator->Edit(
                     NextNotification(collaborator->name(), &processed_version,
                                      collaborator->push_delay())));
  }
}
