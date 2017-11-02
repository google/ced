#pragma once

#include <memory>
#include "buffer.h"

class LibClangCollaborator final : public Collaborator {
 public:
  LibClangCollaborator(const Buffer* buffer);
  ~LibClangCollaborator();

  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
