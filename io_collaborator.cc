// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "io_collaborator.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "temp_file.h"
#include "wrap_syscall.h"

IOCollaborator::IOCollaborator(const Buffer* buffer)
    : Collaborator("io", absl::Milliseconds(100)),
      filename_(buffer->filename()),
      last_char_id_(String::Begin()) {
  fd_ = WrapSyscall("open",
                    [this]() { return open(filename_.c_str(), O_RDONLY); });
  struct stat st;
  WrapSyscall("fstat", [&]() { return fstat(fd_, &st); });
  attributes_ = st.st_mode;
}

void IOCollaborator::Push(const EditNotification& notification) {
  if (!notification.fully_loaded) return;
  absl::MutexLock lock(&mu_);
  if (last_saved_.SameIdentity(notification.content)) return;
  NamedTempFile tmp;
  int fd = WrapSyscall("open", [&]() {
    return open(tmp.filename().c_str(), O_WRONLY | O_CREAT, attributes_);
  });
  try {
    auto str = notification.content.Render();
    WrapSyscall("write", [&]() { return write(fd, str.data(), str.length()); });
  } catch (...) {
    close(fd);
    throw;
  }
  WrapSyscall("rename", [&]() {
    return rename(tmp.filename().c_str(), filename_.c_str());
  });
}

EditResponse IOCollaborator::Pull() {
  static constexpr const int kChunkSize = 4096;
  char buf[kChunkSize];
  const int n = WrapSyscall(
      "read", [this, &buf]() { return read(fd_, buf, sizeof(buf)); });

  EditResponse r;

  if (n != sizeof(buf)) {
    r.done = true;
    r.become_loaded = true;
    close(fd_);
    fd_ = 0;
  }

  absl::MutexLock lock(&mu_);

  last_char_id_ = String::MakeRawInsert(&r.content, site(), std::string(buf, n),
                                        last_char_id_, String::End());

  return r;
}
