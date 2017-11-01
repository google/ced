#pragma once

#include "batching_function.h"
#include "buffer.h"

class ClangFormatCollaborator final : public Collaborator {
 public:
  ClangFormatCollaborator()
      : push_batcher_(1000, [this](String s) { PushBatched(s); }),
        shutdown_(false) {}

  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

 private:
  BatchingFunction<String> push_batcher_;
  void PushBatched(String s);

  absl::Mutex mu_;
  bool shutdown_ GUARDED_BY(mu_);
  CommandBuf commands_ GUARDED_BY(mu_);
};
