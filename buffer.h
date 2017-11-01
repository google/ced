#pragma once

#include <thread>
#include "absl/synchronization/mutex.h"
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
  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;
  virtual void Shutdown() = 0;
  virtual double PushDelay() = 0;

  Site* site() { return &site_; }

 private:
  Site site_;
};

typedef std::unique_ptr<Collaborator> CollaboratorPtr;

class Buffer {
 public:
  Buffer();
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
  String content_ GUARDED_BY(mu_);
  std::vector<CollaboratorPtr> collaborators_ GUARDED_BY(mu_);
  std::vector<std::thread> collaborator_threads_ GUARDED_BY(mu_);
};
