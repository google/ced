#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class FSWatcher {
 public:
  FSWatcher();
  ~FSWatcher();

  void NotifyOnChange(const std::vector<std::string>& interest_set,
                      std::function<void(bool shutting_down)> callback);

 private:
  static constexpr int kWriteEnd = 1;
  static constexpr int kReadEnd = 0;
  int pipe_[2];
  std::unique_ptr<std::thread> watch_;
};
