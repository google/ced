#pragma once

#include "wrap_syscall.h"

class NamedTempFile {
 public:
  NamedTempFile() {
    const char* temp = getenv("TEMP");
    char tpl[1024];
    if (!temp) temp = "/tmp";
    sprintf(tpl, "%s/ced.XXXXXX", temp);
    WrapSyscall("mkstemp", [&]() { return mkstemp(tpl); });
    filename_ = tpl;
  }

  ~NamedTempFile() { unlink(filename_.c_str()); }

  NamedTempFile(const NamedTempFile&) = delete;
  NamedTempFile& operator=(const NamedTempFile&) = delete;

  const std::string& filename() const { return filename_; }

 private:
  std::string filename_;
};
