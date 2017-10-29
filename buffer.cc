#include "buffer.h"

namespace ced {
namespace buffer {

Buffer::Buffer() : shutdown_(false), version_(0), updating_(false) {}

Buffer::~Buffer() {
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
  collaborator_threads_.emplace_back([this, raw]() { RunPull(raw); });
  collaborator_threads_.emplace_back([this, raw]() { RunPush(raw); });
}

static bool IsEmpty(const EditResponse& response) {
  for (const auto& cmdp : response.commands) {
    if (!cmdp.second.empty()) return false;
  }
  return true;
}

void Buffer::RunPush(Collaborator* collaborator) {
  uint64_t processed_version = 0;
  auto processable = [this, &processed_version]() {
    mu_.AssertHeld();
    return shutdown_ || version_ != processed_version;
  };
  for (;;) {
    // wait until something interesting to work on
    mu_.LockWhen(absl::Condition(&processable));
    if (shutdown_) {
      mu_.Unlock();
      return;
    }
    processed_version = version_;
    EditNotification notification{
        views_,
    };
    mu_.Unlock();

    // magick happens here
    collaborator->Push(notification);
  }
}

void Buffer::RunPull(ced::buffer::Collaborator* collaborator) {
  auto updatable = [this]() {
    mu_.AssertHeld();
    return shutdown_ || !updating_;
  };

  for (;;) {
    EditResponse response = collaborator->Pull();

    if (!IsEmpty(response)) {
      // get the update lock
      mu_.LockWhen(absl::Condition(&updatable));
      if (shutdown_) {
        mu_.Unlock();
        return;
      }
      updating_ = true;
      auto views = views_;
      mu_.Unlock();

      for (const auto& cmdp : response.commands) {
        ElemString s = *views.Lookup(cmdp.first);
        for (const auto& cmd : cmdp.second) {
          s = s.Integrate(cmd);
        }
        views = views.Add(cmdp.first, s);
      }

      // commit the update and advance time
      mu_.Lock();
      updating_ = false;
      version_++;
      auto old_views = views_;  // destruct outside lock
      views_ = views;
      mu_.Unlock();
    }
  }
}

}  // namespace buffer
}  // namespace ced
