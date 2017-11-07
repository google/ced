#include "side_buffer.h"

void SideBuffer::CalcLines() {
  line_ofs.clear();
  line_ofs.push_back(0);
  for (auto it = content.begin(); it != content.end(); ++it) {
    if (*it == '\n') {
      line_ofs.push_back(1 + (it - content.begin()));
    }
  }
}
