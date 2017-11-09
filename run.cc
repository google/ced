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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
#include "wrap_syscall.h"

RunResult run(const std::string& command, const std::vector<std::string>& args,
              const std::string& input) {
  enum Pipe { IN, OUT, ERR };
  enum Dir { READ, WRITE };
  int pipes[3][2];
  for (int i = 0; i < 3; i++) {
    WrapSyscall("pipe", [&]() { return pipe(pipes[i]); });
  }

  pid_t p = WrapSyscall("fork", [&]() { return fork(); });
  if (p == 0) {
    WrapSyscall("dup2", [&]() { return dup2(pipes[IN][READ], STDIN_FILENO); });
    WrapSyscall("dup2",
                [&]() { return dup2(pipes[OUT][WRITE], STDOUT_FILENO); });
    WrapSyscall("dup2",
                [&]() { return dup2(pipes[ERR][WRITE], STDERR_FILENO); });
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 2; j++) {
        close(pipes[i][j]);
      }
    }
    std::vector<char*> cargs;
    cargs.push_back(strdup(command.c_str()));
    for (auto& arg : args) cargs.push_back(strdup(arg.c_str()));
    cargs.push_back(nullptr);
    WrapSyscall("execvp", [&]() { return execvp(cargs[0], cargs.data()); });
    abort();
  } else {
    close(pipes[IN][READ]);
    close(pipes[OUT][WRITE]);
    close(pipes[ERR][WRITE]);
    std::thread wr([&]() {
      if (input.length() > 0) {
        WrapSyscall("write", [&]() {
          return write(pipes[IN][WRITE], input.data(), input.length());
        });
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
    };
    RunResult result;
    std::thread rdout([&]() { rd(pipes[OUT][READ], &result.out); });
    std::thread rderr([&]() { rd(pipes[ERR][READ], &result.err); });
    wr.join();
    rdout.join();
    rderr.join();
    waitpid(p, &result.status, 0);
    return result;
  }
}
