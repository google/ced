#include "cppfilt.h"
#include "run.h"
std::string cppfilt(absl::string_view text) {
  return run("c++filt", {}, std::string(text.data(), text.length()));
}

