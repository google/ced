#pragma once

#include "buffer.h"

class GodboltCollaborator final : public SyncCollaborator {
 public:
  GodboltCollaborator(const Buffer* buffer)
      : SyncCollaborator("godbolt", absl::Seconds(0)),
        buffer_(buffer),
        side_buffer_editor_(site()) {}

  EditResponse Edit(const EditNotification& notification) override;

 private:
  const Buffer* const buffer_;
  UMapEditor<std::string, SideBuffer> side_buffer_editor_;
};
