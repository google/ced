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

#include <boost/filesystem.hpp>
#include <thread>
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/optional.h"
#include "annotated_string.h"
#include "selector.h"

struct EditNotification {
  bool fully_loaded = false;
  bool shutdown = false;
  uint64_t referenced_file_version = 0;
  AnnotatedString content;
};

struct EditResponse {
  bool done = false;
  bool become_used = false;
  bool become_loaded = false;
  bool referenced_file_changed = false;
  CommandSet content_updates;
};

void IntegrateResponse(const EditResponse& response, EditNotification* state);

class Collaborator {
 public:
  virtual ~Collaborator(){};

  const char* name() const { return name_; }
  absl::Duration push_delay_from_idle() const { return push_delay_from_idle_; }
  absl::Duration push_delay_from_start() const {
    return push_delay_from_start_;
  }

  Site* site() { return &site_; }

  void MarkRequest() { last_request_ = absl::Now(); }
  void MarkResponse() { last_response_ = absl::Now(); }
  void MarkChange() { last_change_ = absl::Now(); }

  const absl::Time& last_response() const { return last_response_; }
  const absl::Time& last_request() const { return last_request_; }
  const absl::Time& last_change() const { return last_change_; }

 protected:
  Collaborator(const char* name, absl::Duration push_delay_from_idle,
               absl::Duration push_delay_from_start)
      : name_(name),
        push_delay_from_idle_(push_delay_from_idle),
        push_delay_from_start_(push_delay_from_start) {}

 private:
  const char* const name_;
  const absl::Duration push_delay_from_idle_;
  const absl::Duration push_delay_from_start_;
  absl::Time last_response_ = absl::Now();
  absl::Time last_request_ = absl::Now();
  absl::Time last_change_ = absl::Now();
  absl::Duration last_notify_ = absl::Seconds(0);
  Site site_;
};

typedef std::unique_ptr<Collaborator> CollaboratorPtr;

class AsyncCollaborator : public Collaborator {
 public:
  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;

 protected:
  AsyncCollaborator(const char* name, absl::Duration push_delay_from_idle,
                    absl::Duration push_delay_from_start)
      : Collaborator(name, push_delay_from_idle, push_delay_from_start) {}
};

// a collaborator that only makes edits in response to edits
class SyncCollaborator : public Collaborator {
 public:
  virtual EditResponse Edit(const EditNotification& notification) = 0;

 protected:
  SyncCollaborator(const char* name, absl::Duration push_delay_from_idle,
                   absl::Duration push_delay_from_start)
      : Collaborator(name, push_delay_from_idle, push_delay_from_start) {}
};

typedef std::unique_ptr<AsyncCollaborator> AsyncCollaboratorPtr;
typedef std::unique_ptr<SyncCollaborator> SyncCollaboratorPtr;

enum class Language {
  Unknown,
  C,
  Cpp,
  Asm,
};

class Buffer {
 public:
  Buffer(const boost::filesystem::path& filename,
         absl::optional<AnnotatedString> initial_string =
             absl::optional<AnnotatedString>());
  ~Buffer();

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  template <class T, typename... Args>
  T* MakeCollaborator(Args&&... args) {
    T* p = new T(const_cast<const Buffer*>(this), std::forward<Args>(args)...);
    AddCollaborator(std::unique_ptr<T>(p));
    return p;
  }

  const boost::filesystem::path& filename() const { return filename_; }
  Language language() const;
  bool read_only() const { return false; }
  bool synthetic() const { return synthetic_; }

  std::vector<std::string> ProfileData() const;

  static void RegisterCollaborator(
      std::function<void(Buffer*)> maybe_init_collaborator);

  void PushChanges(const CommandSet* cmds);
  AnnotatedString ContentSnapshot();

 private:
  void AddCollaborator(AsyncCollaboratorPtr&& collaborator);
  void AddCollaborator(SyncCollaboratorPtr&& collaborator);

  EditNotification NextNotification(Collaborator* collaborator,
                                    uint64_t* last_processed);
  void SinkResponse(Collaborator* collaborator, const EditResponse& response);

  void RunPush(AsyncCollaborator* collaborator);
  void RunPull(AsyncCollaborator* collaborator);
  void RunSync(SyncCollaborator* collaborator);

  void UpdateState(Collaborator* collaborator, bool become_used,
                   std::function<void(EditNotification& new_state)>);

  mutable absl::Mutex mu_;
  const bool synthetic_;
  uint64_t version_ GUARDED_BY(mu_);
  std::set<Collaborator*> declared_no_edit_collaborators_ GUARDED_BY(mu_);
  std::set<Collaborator*> done_collaborators_ GUARDED_BY(mu_);
  bool updating_ GUARDED_BY(mu_);
  absl::Time last_used_ GUARDED_BY(mu_);
  const boost::filesystem::path filename_;
  EditNotification state_ GUARDED_BY(mu_);
  std::vector<CollaboratorPtr> collaborators_ GUARDED_BY(mu_);
  std::vector<std::thread> collaborator_threads_ GUARDED_BY(mu_);
  std::thread init_thread_;
};

#define IMPL_COLLABORATOR(name, buffer_arg)             \
  namespace {                                           \
  class name##_impl {                                   \
   public:                                              \
    static bool ShouldAdd(const Buffer* buffer_arg);    \
    name##_impl() {                                     \
      Buffer::RegisterCollaborator([](Buffer* buffer) { \
        if (ShouldAdd(buffer)) {                        \
          buffer->MakeCollaborator<name>();             \
        }                                               \
      });                                               \
    }                                                   \
  };                                                    \
  name##_impl impl;                                     \
  }                                                     \
  bool name##_impl::ShouldAdd(const Buffer* buffer_arg)
