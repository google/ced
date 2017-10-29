#include "io_collaborator.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "wrap_syscall.h"

namespace ced {
namespace buffer {

IOCollaborator::IOCollaborator(const std::string& filename)
    : filename_(filename), last_char_id_(ElemString::Begin()) {
  fd_ = tl::WrapSyscall(
      "open", [filename]() { return open(filename.c_str(), O_RDONLY); });
}

void IOCollaborator::Push(const EditNotification& notification) {
  absl::MutexLock lock(&mu_);
  if (!finished_read_) return;
  //  if (notification.views["main"].Contains(*last_char_id_)) {
  //  }
}

EditResponse IOCollaborator::Pull() {
  char buf[1024];
  const int n = tl::WrapSyscall(
      "read", [this, &buf]() { return read(fd_, buf, sizeof(buf)); });

  EditResponse r;

  if (n != sizeof(buf)) {
    r.done = true;
    close(fd_);
    fd_ = 0;
  }

  absl::MutexLock lock(&mu_);
  finished_read_ = r.done;

  auto& commands = r.commands["main"];
  for (int i = 0; i < n; i++) {
    auto cmd = ElemString::MakeRawInsert(site(), buf[i], last_char_id_,
                                         ElemString::End());
    last_char_id_ = cmd->id();
    commands.emplace_back(std::move(cmd));
  }

  return r;
}

}  // namespace buffer
}  // namespace ced
