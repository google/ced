#pragma once

#include "buffer.h"

class IOCollaborator final : public Collaborator {
 public:
  IOCollaborator(const std::string& filename);
  virtual void Push(const EditNotification& notification) override;
  virtual EditResponse Pull() override;

 private:
  absl::Mutex mu_;
  const std::string filename_;
  int fd_;
  bool finished_read_ GUARDED_BY(mu_);
  ID last_char_id_ GUARDED_BY(mu_);
};
