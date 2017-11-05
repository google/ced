#pragma once

#include "buffer.h"

class ClangFormatCollaborator final : public SyncCollaborator {
 public:
  ClangFormatCollaborator(const Buffer* buffer)
      : SyncCollaborator("clang-format", absl::Seconds(2)) {}

  EditResponse Edit(const EditNotification& notification) override;
};
