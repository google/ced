#pragma once

#include "absl/synchronization/mutex.h"
#include "buffer.h"
#include "fullscreen.h"

namespace ced {
namespace buffer {

class TerminalCollaborator final : public Collaborator {
 public:
  TerminalCollaborator(tl::Fullscreen* fullscreen);
  virtual void Push(const EditNotification& notification) override;
  virtual EditResponse Pull() override;

  void Render(tl::FrameBuffer* fb);

 private:
  tl::Fullscreen* const terminal_;
  absl::Mutex mu_;
  functional_util::AVL<std::string, ElemString> last_seen_views_
      GUARDED_BY(mu_);
  woot::ID cursor_ GUARDED_BY(mu_);
  int cursor_row_ GUARDED_BY(mu_);
};

}  // namespace buffer
}  // namespace ced
