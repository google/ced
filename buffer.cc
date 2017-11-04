#include "buffer.h"
#include "io_collaborator.h"
#include "log.h"

Buffer::Buffer(const std::string& filename)
    : shutdown_(false),
      fully_loaded_(false),
      version_(0),
      updating_(false),
      last_used_(absl::Now() - absl::Seconds(1000000)),
      filename_(filename) {
  MakeCollaborator<IOCollaborator>();
}

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
  collaborator_threads_.emplace_back([this, raw]() {
    try {
      RunPull(raw);
    } catch (std::exception& e) {
      Log() << "collaborator pull broke: " << e.what();
    }
  });
  collaborator_threads_.emplace_back([this, raw]() {
    try {
      RunPush(raw);
    } catch (std::exception& e) {
      Log() << "collaborator push broke: " << e.what();
    }
  });
}

void Buffer::RunPush(Collaborator* collaborator) {
  uint64_t processed_version = 0;
  auto processable = [this, &processed_version]() {
    mu_.AssertHeld();
    return shutdown_ || version_ != processed_version;
  };
  bool first = true;
  for (;;) {
    // wait until something interesting to work on
    mu_.LockWhen(absl::Condition(&processable));
    absl::Time last_used_at_start;
    do {
      Log() << collaborator->name() << " last_used: " << last_used_;
      last_used_at_start = last_used_;
      absl::Duration idle_time = absl::Now() - last_used_;
      Log() << collaborator->name() << " idle_time: " << idle_time;
      if (!first &&
          mu_.AwaitWithTimeout(absl::Condition(&shutdown_),
                               collaborator->push_delay() - idle_time)) {
        mu_.Unlock();
        return;
      }
    } while (last_used_ != last_used_at_start);
    processed_version = version_;
    EditNotification notification{
        content_,
        fully_loaded_,
    };
    mu_.Unlock();

    // magick happens here
    Log() << collaborator->name() << " push";
    collaborator->Push(notification);
    first = false;
  }
}

static bool HasUpdates(const EditResponse& response) {
  return response.become_loaded || !response.commands.empty();
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
      if (response.become_used) {
        last_used_ = absl::Now();
      }
      if (response.become_loaded) {
        fully_loaded_ = true;
      }
      content_ = content;
      mu_.Unlock();
    } else {
      absl::MutexLock lock(&mu_);
      if (response.become_used) {
        last_used_ = absl::Now();
      }
      if (shutdown_) {
        return;
      }
    }

    if (response.done) {
      return;
    }
  }
}
