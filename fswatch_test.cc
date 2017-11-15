#include "fswatch.h"
#include <stdio.h>

int main(int argc, char** argv) {
  std::vector<std::string> s;
  for (int i = 1; i < argc; i++) {
    s.push_back(argv[i]);
  }
  FSWatcher w;
  std::function<void(bool)> cb = [&](bool shutdown) {
    if (!shutdown) {
      puts("file changed");
      w.NotifyOnChange(s, cb);
    } else {
      puts("shutdown");
    }
  };
  w.NotifyOnChange(s, cb);
  getchar();
}
