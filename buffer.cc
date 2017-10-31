#include "buffer.h"

Buffer::Buffer() : shutdown_(false), version_(0), updating_(false) {}

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
  collaborator_threads_.emplace_back([this, raw]() { RunPull(raw); });
  collaborator_threads_.emplace_back([this, raw]() { RunPush(raw); });
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
        content_,
    };
    mu_.Unlock();

    // magick happens here
    collaborator->Push(notification);
  }
}

static bool HasUpdates(const EditResponse& response) {
  return !response.commands.empty();
}

void Buffer::RunPull(Collaborator* collaborator) {
  auto updatable = [this]() {
    mu_.AssertHeld();
    return shutdown_ || !updating_;
  };

  for (;;) {
    EditResponse response = collaborator->Pull();

    if (HasUpdates(response)) {
      // get the update lock
      mu_.LockWhen(absl::Condition(&updatable));
      if (shutdown_) {
        mu_.Unlock();
        return;
      }
      updating_ = true;
      auto content = content_;
      mu_.Unlock();

      for (const auto& cmd : response.commands) {
        content = content.Integrate(cmd);
      }

      // commit the update and advance time
      mu_.Lock();
      updating_ = false;
      version_++;
      auto old_content = content_;  // destruct outside lock
      content_ = content;
      mu_.Unlock();
    }

    if (response.done) {
      return;
    }
  }
}
