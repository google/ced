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
