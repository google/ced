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
#include "wrap_syscall.h"
#include <errno.h>
#include <stdexcept>
#include "absl/strings/str_cat.h"

int WrapSyscall(const char* name, std::function<int()> fn) {
  int r;
  for (;;) {
    r = fn();
    if (r == -1) {
      int e = errno;
      if (e == EINTR) continue;
      throw std::runtime_error(
          absl::StrCat(name, " failed: errno=", e, " ", strerror(e)));
    }
    return r;
  }
}
