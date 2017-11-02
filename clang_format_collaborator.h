#pragma once

#include "buffer.h"

class ClangFormatCollaborator final : public Collaborator {
 public:
  ClangFormatCollaborator()
      : Collaborator("clang-format", absl::Seconds(2)), shutdown_(false) {}

  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

 private:
  absl::Mutex mu_;
  bool shutdown_ GUARDED_BY(mu_);
  CommandBuf commands_ GUARDED_BY(mu_);
};
