#pragma once

#include "buffer.h"

// Guard against repeating processing on the same data
class ContentLatch {
 public:
  ContentLatch(bool consumes_dependents)
      : consumes_dependents_(consumes_dependents) {}

  bool IsNewContent(const EditNotification& notification) {
    if (notification.content.SameIdentity(last_str_)) {
      if (!consumes_dependents_) {
        return false;
      } else if (last_deps_ == notification.referenced_file_version) {
        return false;
      }
    }
    last_str_ = notification.content;
    last_deps_ = notification.referenced_file_version;
    return true;
  }

 private:
  const bool consumes_dependents_;
  String last_str_;
  uint64_t last_deps_ = 0;
};
