#include "godbolt_collaborator.h"
#include "log.h"
#include "subprocess.hpp"

using namespace subprocess;

void GodboltCollaborator::Push(const EditNotification& notification) {
  if (!notification.fully_loaded) return;
  auto str = notification.content;
  auto text = str.Render();
  auto p = Popen({"clang-format", "-output-replacements-xml"}, input{PIPE},
                 output{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();

  Log() << res.first.buf.data();
}

EditResponse GodboltCollaborator::Pull() {
  auto ready = [this]() {
    mu_.AssertHeld();
    return shutdown_ || !commands_.empty();
  };
  EditResponse r;
  mu_.LockWhen(absl::Condition(&ready));
  r.commands.swap(commands_);
  r.done = shutdown_;
  mu_.Unlock();
  return r;
}

void GodboltCollaborator::Shutdown() {
  absl::MutexLock lock(&mu_);
  shutdown_ = true;
}
