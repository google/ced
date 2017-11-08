#include "run.h"
#include <thread>
#include "include/subprocess.hpp"

std::string run(const std::string& command,
                const std::vector<std::string>& args,
                const std::string& input) {
  auto p = subprocess::popen(command, args);
  std::thread t([&]() {
    p.stdin() << input;
    p.close();
  });
  std::string result(std::istreambuf_iterator<char>(p.stdout()),
                     std::istreambuf_iterator<char>());
  t.join();
  return result;
}
