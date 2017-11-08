#include "cppfilt.h"
#include "subprocess.hpp"

using namespace subprocess;

std::string cppfilt(absl::string_view text) {
  auto p = Popen("c++filt", input{PIPE}, output{PIPE}, error{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();
  return res.first.buf.data();
}

