#include "run.h"
#include "include/subprocess.hpp"
#include <thread>

std::string run(const std::string& command, const std::vector<std::string>& args, const std::string& input) {
  auto p = subprocess::popen(command, args);
  std::thread t([&]() {
    p.stdin() << input;
    p.close();
  });
  std::string result(std::istreambuf_iterator<char>(p.stdout()), std::istreambuf_iterator<char>());
  t.join();
  return result;
}

