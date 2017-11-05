#pragma once

#include <memory>
#include "buffer.h"

class LibClangCollaborator final : public SyncCollaborator {
 public:
  LibClangCollaborator(const Buffer* buffer);
  ~LibClangCollaborator();

  EditResponse Edit(const EditNotification& notification) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
