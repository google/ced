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
#include "fswatch.h"
#include <assert.h>

namespace {
class Cleanup {
 public:
  void Add(std::function<void()> fn) { fns_.push_back(fn); }
  ~Cleanup() {
    for (auto& fn : fns_) fn();
  }

 private:
  std::vector<std::function<void()>> fns_;
};
}  // namespace

#ifdef __APPLE__
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
bool RunWatch(int pipe_read, const std::vector<std::string>& interest_set) {
  Cleanup cleanup;
  int kq = -1;

  if ((kq = kqueue()) < 0) {
    fprintf(stderr, "Could not open kernel queue.  Error was %s.\n",
            strerror(errno));
    return false;
  }
  cleanup.Add([=]() { close(kq); });

  std::vector<int> event_fds(interest_set.size());
  std::vector<struct kevent> events_to_monitor(interest_set.size() + 1);
  for (size_t i = 0; i < interest_set.size(); i++) {
    const auto& path = interest_set[i];
    int efd = open(path.c_str(), O_EVTONLY);
    if (efd <= 0) {
      fprintf(
          stderr,
          "The file %s could not be opened for monitoring.  Error was %s.\n",
          path.c_str(), strerror(errno));
      return false;
    }
    event_fds[i] = efd;
    cleanup.Add([=]() { close(efd); });

    /* Set up a list of events to monitor. */
    unsigned vnode_events =
        NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME | NOTE_REVOKE;
    EV_SET(&events_to_monitor[i], event_fds[i], EVFILT_VNODE, EV_ADD | EV_CLEAR,
           vnode_events, 0, nullptr);
  }
  EV_SET(&events_to_monitor[interest_set.size()], pipe_read, EVFILT_READ,
         EV_ADD, 0, 0, &pipe_read);

  std::vector<struct kevent> event_data(interest_set.size() + 1);

  int event_count =
      kevent(kq, events_to_monitor.data(), events_to_monitor.size(),
             event_data.data(), event_data.size(), nullptr);
  if (event_count < 0) {
    /* An error occurred. */
    fprintf(stderr, "An error occurred (event count %d).  The error was %s.\n",
            event_count, strerror(errno));
    return false;
  }
  for (int i = 0; i < event_count; i++) {
    if (event_data[i].udata == &pipe_read) return false;
  }
  return true;
}
}  // namespace
#else  // assume linux
#include <limits.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

bool RunWatch(int pipe_read, const std::vector<std::string>& interest_set) {
  int inotify_fd = inotify_init();
  Cleanup cleanup;
  if (inotify_fd < 0) {
    fprintf(stderr, "Failed to create inotify fd\n");
    return false;
  }
  cleanup.Add([=]() { close(inotify_fd); });
  for (const auto& path : interest_set) {
    int r = inotify_add_watch(inotify_fd, path.c_str(), IN_MODIFY);
    if (r == -1) {
      fprintf(stderr, "inotify_add_watch %s failed\n", path.c_str());
      return false;
    }
  }
  struct pollfd poll_fds[] = {
      {inotify_fd, POLLIN, 0},
      {pipe_read, POLLIN, 0},
  };
  poll(poll_fds, sizeof(poll_fds) / sizeof(*poll_fds), -1);
  return poll_fds[1].revents == 0;
}
#endif

FSWatcher::FSWatcher(const std::vector<std::string>& interest_set,
                     std::function<void(bool)> callback) {
  if (pipe(pipe_) < 0) throw std::runtime_error("Failed to create pipe");
  watch_ = std::thread([=]() {
    bool sd = !RunWatch(pipe_[kReadEnd], interest_set);
    std::thread([=]() { callback(sd); }).detach();
  });
}

FSWatcher::~FSWatcher() {
  char c;
  write(pipe_[kWriteEnd], &c, 1);
  watch_.join();
  close(pipe_[kWriteEnd]);
  close(pipe_[kReadEnd]);
}
