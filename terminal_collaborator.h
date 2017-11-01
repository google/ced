#pragma once

#include "absl/synchronization/mutex.h"
#include "buffer.h"

class TerminalCollaborator final : public Collaborator {
 public:
  TerminalCollaborator(std::function<void()> invalidate);
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;
  void Shutdown() override;

  void Render();
  void ProcessKey(int key);

 private:
  const std::function<void()> used_;
  absl::Mutex mu_;
  std::vector<String::CommandPtr> commands_ GUARDED_BY(mu_);
  bool recently_used_ GUARDED_BY(mu_);
  bool shutdown_ GUARDED_BY(mu_);
  String content_ GUARDED_BY(mu_);
  ID cursor_ GUARDED_BY(mu_);
  int cursor_row_ GUARDED_BY(mu_);
};
