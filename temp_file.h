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

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "wrap_syscall.h"

class NamedTempFile {
 public:
  NamedTempFile() {
    const char* temp = getenv("TEMP");
    char tpl[1024];
    if (!temp) temp = "/tmp";
    sprintf(tpl, "%s/ced.XXXXXX", temp);
    close(WrapSyscall("mkstemp", [&]() { return mkstemp(tpl); }));
    filename_ = tpl;
  }

  ~NamedTempFile() { unlink(filename_.c_str()); }

  NamedTempFile(const NamedTempFile&) = delete;
  NamedTempFile& operator=(const NamedTempFile&) = delete;

  const std::string& filename() const { return filename_; }

 private:
  std::string filename_;
};
