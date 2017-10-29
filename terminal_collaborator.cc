#include "terminal_collaborator.h"

namespace ced {
namespace buffer {

TerminalCollaborator::TerminalCollaborator(tl::Fullscreen* terminal)
    : terminal_(terminal) {}

void TerminalCollaborator::Push(const EditNotification& notification) {
  {
    absl::MutexLock lock(&mu_);
    last_seen_views_ = notification.views;
  }
  terminal_->Invalidate();
}

EditResponse TerminalCollaborator::Pull() {
  EditResponse r;
  r.done = true;
  return r;
}

}  // namespace buffer
}  // namespace ced
