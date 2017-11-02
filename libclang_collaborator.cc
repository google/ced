#include "libclang_collaborator.h"
#include "clang-c/Index.h"

namespace {

class ClangEnv {
 public:
  static ClangEnv* Get() {
    static ClangEnv env;
    return &env;
  }

  ~ClangEnv() { clang_disposeIndex(index_); }

  absl::Mutex* mu() { return &mu_; }

 private:
  ClangEnv() { index_ = clang_createIndex(0, 0); }

  absl::Mutex mu_;
  CXIndex index_ GUARDED_BY(mu_);
};

}  // namespace

LibClangCollaborator::LibClangCollaborator()
    : Collaborator("libclang", absl::Seconds(0)),
      mu_(ClangEnv::Get()->mu()),
      shutdown_(false) {}

void LibClangCollaborator::Push(const EditNotification& notification) {}

EditResponse LibClangCollaborator::Pull() {
  EditResponse r;
  r.done = true;
  return r;
}

void LibClangCollaborator::Shutdown() {}
