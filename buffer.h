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

#include <thread>
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/any.h"
#include "diagnostic.h"
#include "selector.h"
#include "side_buffer.h"
#include "umap.h"
#include "uset.h"
#include "woot.h"

template <class T>
struct Annotation {
  Annotation(ID e, T&& d) : end(e), data(std::move(d)) {}
  Annotation(ID e, const T& d) : end(e), data(d) {}
  ID end;
  T data;
  bool operator<(const Annotation& rhs) const {
    return end < rhs.end || (end == rhs.end && data < rhs.data);
  }
};

template <class T>
using AnnotationMap = UMap<ID, Annotation<T>>;

template <class T>
class AnnotationTracker {
 public:
  AnnotationTracker(const AnnotationMap<T>& map) : map_(map) {}

  void Enter(ID id) {
    active_.erase(
        std::remove_if(active_.begin(), active_.end(),
                       [id](const Annotation<T>& a) { return id == a.end; }),
        active_.end());
    map_.ForEachValue(
        id, [&](const Annotation<T>& ann) { active_.push_back(ann); });
  }

  T cur() {
    if (active_.empty()) return T();
    return active_.back().data;
  }

 private:
  AnnotationMap<T> map_;
  std::vector<Annotation<T>> active_;
};

struct SideBufferRef {
  std::string name;
  std::vector<int> lines;

  bool operator<(const SideBufferRef& other) const {
    return name < other.name || (name == other.name && lines < other.lines);
  }
};

template <template <class Type> class TypeTranslator>
struct EditState {
  TypeTranslator<String> content;
  TypeTranslator<AnnotationMap<Tag>> token_types;
  TypeTranslator<USet<Diagnostic>> diagnostics;
  TypeTranslator<AnnotationMap<ID>> diagnostic_ranges;
  TypeTranslator<UMap<std::string, SideBuffer>> side_buffers;
  TypeTranslator<AnnotationMap<SideBufferRef>> side_buffer_refs;
  TypeTranslator<USet<Fixit>> fixits;
  TypeTranslator<UMap<ID, std::string>> gutter_notes;
  TypeTranslator<USet<std::string>> referenced_files;
  TypeTranslator<USet<ID>> cursors;
};

template <class T>
using NotifTrans = T;

struct EditNotification : public EditState<NotifTrans> {
  bool fully_loaded = false;
  bool shutdown = false;
  uint64_t referenced_file_version = 0;
};

template <class T>
using RspTrans = typename T::CommandBuf;

struct EditResponse : public EditState<RspTrans> {
  bool done = false;
  bool become_used = false;
  bool become_loaded = false;
  bool referenced_file_changed = false;
};

class Collaborator {
 public:
  const char* name() const { return name_; }
  absl::Duration push_delay() const { return push_delay_; }

  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;

  Site* site() { return &site_; }

 protected:
  Collaborator(const char* name, absl::Duration push_delay)
      : name_(name), push_delay_(push_delay) {}

 private:
  const char* const name_;
  const absl::Duration push_delay_;
  Site site_;
};

typedef std::unique_ptr<Collaborator> CollaboratorPtr;

// a collaborator that only makes edits in response to edits
class SyncCollaborator {
 public:
  const char* name() const { return name_; }
  absl::Duration push_delay() const { return push_delay_; }

  virtual EditResponse Edit(const EditNotification& notification) = 0;

  Site* site() { return &site_; }

 protected:
  SyncCollaborator(const char* name, absl::Duration push_delay)
      : name_(name), push_delay_(push_delay) {}

 private:
  const char* const name_;
  const absl::Duration push_delay_;
  Site site_;
};

typedef std::unique_ptr<SyncCollaborator> SyncCollaboratorPtr;

class Buffer {
 public:
  Buffer(const std::string& filename);
  ~Buffer();

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  template <class T, typename... Args>
  T* MakeCollaborator(Args&&... args) {
    T* p = new T(const_cast<const Buffer*>(this), std::forward<Args>(args)...);
    AddCollaborator(std::unique_ptr<T>(p));
    return p;
  }

  const std::string& filename() const { return filename_; }

 private:
  void AddCollaborator(CollaboratorPtr&& collaborator);
  void AddCollaborator(SyncCollaboratorPtr&& collaborator);

  EditNotification NextNotification(void* id, const char* name,
                                    uint64_t* last_processed,
                                    absl::Duration push_delay);
  void SinkResponse(void* id, const char* name, const EditResponse& response);

  void RunPush(Collaborator* collaborator);
  void RunPull(Collaborator* collaborator);
  void RunSync(SyncCollaborator* collaborator);

  void UpdateState(bool become_used,
                   std::function<void(EditNotification& new_state)>);

  absl::Mutex mu_;
  uint64_t version_ GUARDED_BY(mu_);
  std::set<void*> declared_no_edit_collaborators_ GUARDED_BY(mu_);
  std::set<void*> done_collaborators_ GUARDED_BY(mu_);
  bool updating_ GUARDED_BY(mu_);
  absl::Time last_used_ GUARDED_BY(mu_);
  const std::string filename_ GUARDED_BY(mu_);
  EditNotification state_ GUARDED_BY(mu_);
  std::vector<CollaboratorPtr> collaborators_ GUARDED_BY(mu_);
  std::vector<SyncCollaboratorPtr> sync_collaborators_ GUARDED_BY(mu_);
  std::vector<std::thread> collaborator_threads_ GUARDED_BY(mu_);
};
