#pragma once

#include "absl/synchronization/mutex.h"
#include "buffer.h"

class TerminalCollaborator final : public Collaborator {
 public:
  TerminalCollaborator(std::function<void()> invalidate);
  virtual void Push(const EditNotification& notification) override;
  virtual EditResponse Pull() override;

  void Render();
  void ProcessKey(int key);

 private:
  const std::function<void()> invalidate_;
  absl::Mutex mu_;
  ElemString content_ GUARDED_BY(mu_);
  ID cursor_ GUARDED_BY(mu_);
  int cursor_row_ GUARDED_BY(mu_);
};
