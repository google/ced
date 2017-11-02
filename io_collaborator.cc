#include "io_collaborator.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "wrap_syscall.h"

IOCollaborator::IOCollaborator(const std::string& filename)
    : Collaborator("io", absl::Seconds(1)),
      filename_(filename),
      last_char_id_(String::Begin()) {
  fd_ = WrapSyscall("open",
                    [filename]() { return open(filename.c_str(), O_RDONLY); });
}

void IOCollaborator::Shutdown() {}

void IOCollaborator::Push(const EditNotification& notification) {
  absl::MutexLock lock(&mu_);
  if (!finished_read_) return;
  //  if (notification.views["main"].Contains(*last_char_id_)) {
  //  }
}

EditResponse IOCollaborator::Pull() {
  char buf[1024];
  const int n = WrapSyscall(
      "read", [this, &buf]() { return read(fd_, buf, sizeof(buf)); });

  EditResponse r;

  if (n != sizeof(buf)) {
    r.done = true;
    close(fd_);
    fd_ = 0;
  }

  absl::MutexLock lock(&mu_);
  finished_read_ = r.done;

  for (int i = 0; i < n; i++) {
    auto cmd =
        String::MakeRawInsert(site(), buf[i], last_char_id_, String::End());
    last_char_id_ = cmd->id();
    r.commands.emplace_back(std::move(cmd));
  }

  return r;
}
