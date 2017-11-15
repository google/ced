#include "fswatch.h"
#include <assert.h>

#ifdef __APPLE__
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

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
    unsigned vnode_events = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND |
                            NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME | NOTE_REVOKE;
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
}
#endif

FSWatcher::FSWatcher() {
  if (pipe(pipe_) < 0) throw std::runtime_error("Failed to create pipe");
}

FSWatcher::~FSWatcher() {
  char c;
  write(pipe_[kWriteEnd], &c, 1);
  if (watch_) {
    watch_->join();
    watch_.reset();
  }
  close(pipe_[kWriteEnd]);
  close(pipe_[kReadEnd]);
}

void FSWatcher::NotifyOnChange(const std::vector<std::string>& interest_set,
                               std::function<void(bool)> callback) {
  if (watch_) {
    watch_->join();
    watch_.reset();
  }
  watch_.reset(new std::thread([=]() {
    bool sd = !RunWatch(pipe_[kReadEnd], interest_set);
    std::thread([=]() { callback(sd); }).detach();
  }));
}
