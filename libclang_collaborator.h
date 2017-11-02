#pragma once

#include "buffer.h"

class LibClangCollaborator final : public Collaborator {
 public:
  LibClangCollaborator();

  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

 private:
  absl::Mutex* const mu_;
  bool shutdown_ GUARDED_BY(*mu_);
  // CommandBuf commands_ GUARDED_BY(*mu_);
};
