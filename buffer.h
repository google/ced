#pragma once

#include <thread>
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/any.h"
#include "woot.h"

typedef std::vector<String::CommandPtr> CommandBuf;

struct EditNotification {
  String content;
};

struct EditResponse {
  CommandBuf commands;
  bool done = false;
  bool become_used = false;
};

class Collaborator {
 public:
  const char* name() const { return name_; }
  absl::Duration push_delay() const { return push_delay_; }

  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;
  virtual void Shutdown() = 0;

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

class Buffer {
 public:
  Buffer(const std::string& filename);
  ~Buffer();

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  template <class T, typename... Args>
  T* MakeCollaborator(Args&&... args) {
    T* p = new T(std::forward<Args>(args)...);
    AddCollaborator(CollaboratorPtr(p));
    return p;
  }

 private:
  void AddCollaborator(CollaboratorPtr&& collaborator);

  void RunPush(Collaborator* collaborator);
  void RunPull(Collaborator* collaborator);

  absl::Mutex mu_;
  bool shutdown_ GUARDED_BY(mu_);
  uint64_t version_ GUARDED_BY(mu_);
  bool updating_ GUARDED_BY(mu_);
  absl::Time last_used_ GUARDED_BY(mu_);
  const std::string filename_ GUARDED_BY(mu_);
  String content_ GUARDED_BY(mu_);
  std::vector<CollaboratorPtr> collaborators_ GUARDED_BY(mu_);
  std::vector<std::thread> collaborator_threads_ GUARDED_BY(mu_);
};
