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

  absl::Mutex* mu() LOCK_RETURNED(mu_) { return &mu_; }

 private:
  ClangEnv() { index_ = clang_createIndex(0, 0); }

  absl::Mutex mu_;
  CXIndex index_ GUARDED_BY(mu_);
};

}  // namespace

struct LibClangCollaborator::Impl {
  absl::Mutex* const mu_;

  Impl() : mu_(ClangEnv::Get()->mu()) {}
};

LibClangCollaborator::LibClangCollaborator()
    : Collaborator("libclang", absl::Seconds(0)),
      impl_(new Impl()) {}

LibClangCollaborator::~LibClangCollaborator() {}

void LibClangCollaborator::Push(const EditNotification& notification) {}

EditResponse LibClangCollaborator::Pull() {
  EditResponse r;
  r.done = true;
  return r;
}

void LibClangCollaborator::Shutdown() {}
