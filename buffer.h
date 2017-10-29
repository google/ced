#pragma once

#include <thread>
#include "absl/synchronization/mutex.h"
#include "absl/types/any.h"
#include "woot.h"

namespace ced {
namespace buffer {

typedef absl::any Elem;

typedef woot::String<Elem> ElemString;
typedef std::vector<ElemString::CommandPtr> CommandBuf;

struct EditNotification {
  functional_util::AVL<std::string, ElemString> views;
};

struct EditResponse {
  std::map<std::string, CommandBuf> commands;
  bool done = false;
};

class Collaborator {
 public:
  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;

  woot::Site* site() { return &site_; }

 private:
  woot::Site site_;
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
  functional_util::AVL<std::string, ElemString> views_ GUARDED_BY(mu_);
  std::vector<CollaboratorPtr> collaborators_ GUARDED_BY(mu_);
  std::vector<std::thread> collaborator_threads_ GUARDED_BY(mu_);
};

}  // namespace buffer
}  // namespace ced
