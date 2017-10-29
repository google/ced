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

void TerminalCollaborator::Render(tl::FrameBuffer* fb) {
  absl::MutexLock lock(&mu_);
  const ElemString* s = last_seen_views_.Lookup("main");
  if (!s) return;
  ElemString::Iterator iter(*s);
  int col = 0;
  int row = 0;
  while (!iter.is_end()) {
    if (iter.is_visible()) {
      if (const char* c = absl::any_cast<char>(iter.value())) {
        if (*c == '\n') {
          col = 0;
          row++;
          if (row >= fb->rows()) return;
        } else if (col < fb->cols()) {
          fb->cell(col, row)->set_glyph(*c);
          col++;
        }
      }
    }
    iter.MoveNext();
  }
}

}  // namespace buffer
}  // namespace ced
