#pragma once

#include "buffer.h"

class GodboltCollaborator final : public Collaborator {
 public:
  GodboltCollaborator(const Buffer* buffer)
      : Collaborator("clang-format", absl::Seconds(5)), shutdown_(false) {}

  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

 private:
  absl::Mutex mu_;
  bool shutdown_ GUARDED_BY(mu_);
  CommandBuf commands_ GUARDED_BY(mu_);
};
