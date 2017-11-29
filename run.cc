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
#include "run.h"
#include <string.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "log.h"
#include "wrap_syscall.h"

#ifdef __APPLE__
#define FDS_DIR "/dev/fd"
#else
#define FDS_DIR "/proc/self/fd"
#endif

static void CloseFDsAfter(int after) {
  DIR* d;

  if ((d = opendir(FDS_DIR))) {
    struct dirent* de;

    while ((de = readdir(d))) {
      if (de->d_name[0] == '.') continue;

      errno = 0;
      char* e = NULL;
      long l = strtol(de->d_name, &e, 10);
      if (errno != 0 || !e || *e) {
        closedir(d);
        throw std::runtime_error(absl::StrCat("Confused: saw error ", errno,
                                              " looking at ", FDS_DIR, " file ",
                                              de->d_name));
      }

      auto fd = (int)l;

      if ((long)fd != l) {
        closedir(d);
        throw std::runtime_error(absl::StrCat("Confused: ", fd, " != ", l,
                                              " looking at ", FDS_DIR, " file ",
                                              de->d_name));
      }

      if (fd <= after) continue;

      if (fd == dirfd(d)) continue;

      if (close(fd) < 0) {
        int saved_errno;

        saved_errno = errno;
        closedir(d);
        errno = saved_errno;

        throw std::runtime_error(
            absl::StrCat("Confused: close(", fd, ") failed with errno ", errno,
                         " looking at ", FDS_DIR, " file ", de->d_name));
      }
    }

    closedir(d);
  }
}

RunResult run(const boost::filesystem::path& command,
              const std::vector<std::string>& args, const std::string& input) {
  enum Pipe { IN, OUT, ERR };
  enum Dir { READ, WRITE };
  int pipes[3][2];
  for (int i = 0; i < 3; i++) {
    WrapSyscall("pipe", [&]() { return pipe(pipes[i]); });
  }

  Log() << "RUN: "
        << absl::StrCat(command.string(), " ", absl::StrJoin(args, " "));

  pid_t p = WrapSyscall("fork", [&]() { return fork(); });
  if (p == 0) {
    std::vector<char*> cargs;
    cargs.push_back(strdup(command.string().c_str()));
    for (auto& arg : args) cargs.push_back(strdup(arg.c_str()));
    cargs.push_back(nullptr);
    WrapSyscall("dup2", [&]() { return dup2(pipes[IN][READ], STDIN_FILENO); });
    WrapSyscall("dup2",
                [&]() { return dup2(pipes[OUT][WRITE], STDOUT_FILENO); });
    WrapSyscall("dup2",
                [&]() { return dup2(pipes[ERR][WRITE], STDERR_FILENO); });
    CloseFDsAfter(STDERR_FILENO);
    execvp(cargs[0], cargs.data());
    abort();
  } else {
    close(pipes[IN][READ]);
    close(pipes[OUT][WRITE]);
    close(pipes[ERR][WRITE]);
    std::thread wr([&]() {
      if (input.length() > 0) {
        const char* buf = input.data();
        const char* end = buf + input.length();
        while (buf != end) {
          buf += WrapSyscall("write", [&]() {
            return write(pipes[IN][WRITE], buf, end - buf);
          });
        }
      }
      close(pipes[IN][WRITE]);
    });
    auto rd = [](int fd, std::string* out) {
      char buf[1024];
      int n;
      do {
        n = WrapSyscall("read", [&]() { return read(fd, buf, sizeof(buf)); });
        out->append(buf, n);
      } while (n > 0);
      close(fd);
    };
    RunResult result;
    std::thread rdout([&]() { rd(pipes[OUT][READ], &result.out); });
    std::thread rderr([&]() { rd(pipes[ERR][READ], &result.err); });
    std::thread wait([&]() { waitpid(p, &result.status, 0); });
    wr.join();
    rdout.join();
    rderr.join();
    wait.join();
    return result;
  }
}

void run_daemon(const boost::filesystem::path& command,
                const std::vector<std::string>& args) {
  Log() << "RUN DAEMON: "
        << absl::StrCat(command.string(), " ", absl::StrJoin(args, " "));

  pid_t p = WrapSyscall("fork", [&]() { return fork(); });
  if (p == 0) {
    std::vector<char*> cargs;
    cargs.push_back(strdup(command.string().c_str()));
    for (auto& arg : args) cargs.push_back(strdup(arg.c_str()));
    cargs.push_back(nullptr);
#if 1
    int rdf = WrapSyscall("open", []() { return open("/dev/null", O_RDONLY); });
    int wrf = WrapSyscall("open", []() { return open("/dev/null", O_RDONLY); });
    WrapSyscall("dup2", [&]() { return dup2(rdf, STDIN_FILENO); });
    WrapSyscall("dup2", [&]() { return dup2(wrf, STDOUT_FILENO); });
    WrapSyscall("dup2", [&]() { return dup2(wrf, STDERR_FILENO); });
#endif
    CloseFDsAfter(STDERR_FILENO);
    execvp(cargs[0], cargs.data());
    abort();
  }
}
