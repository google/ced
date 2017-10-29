#pragma once

#include <thread>
#include "absl/synchronization/mutex.h"
#include "woot.h"

namespace ced {
namespace buffer {

class Elem;
typedef std::shared_ptr<Elem> ElemPtr;
class Elem {};

typedef woot::String<ElemPtr> ElemString;
typedef std::vector<ElemString::CommandPtr> CommandBuf;

struct EditNotification {
  functional_util::AVL<std::string, ElemString> views;
};

struct EditResponse {
  std::map<std::string, CommandBuf> commands;
};

class Collaborator {
 public:
  virtual void Push(const EditNotification& notification) = 0;
  virtual EditResponse Pull() = 0;
};

typedef std::unique_ptr<Collaborator> CollaboratorPtr;

class Buffer {
 public:
  Buffer();
  ~Buffer();

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  void AddCollaborator(CollaboratorPtr&& collaborator);

 private:
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
