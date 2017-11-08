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
#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include "wrap_syscall.h"

class Log : public std::ostringstream {
 public:
  Log() = default;
  ~Log() {
    static LogFile file;
    *this << '\n';
    WrapSyscall("write", [this]() {
      auto s = str();
      return ::write(file.hdl(), s.data(), s.length());
    });
  }

  Log(const Log&) = delete;
  Log& operator=(const Log&) = delete;

 private:
  class LogFile {
   public:
    LogFile() {
      std::ostringstream name;
      name << ".cedlog." << getpid();
      auto s = name.str();
      hdl_ = WrapSyscall("open", [s]() {
        return open(s.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0777);
      });
    }

    ~LogFile() { close(hdl_); }

    int hdl() const { return hdl_; }

   private:
    int hdl_;
  };
};

#define Log() \
  if (false)  \
    ;         \
  else        \
    Log()
