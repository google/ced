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
