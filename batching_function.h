#pragma once

#include <functional>
#include <thread>
#include "absl/synchronization/mutex.h"
#include "log.h"

template <class T>
class BatchingFunction {
 public:
  BatchingFunction(int delay_ms, std::function<void(T)> f)
      : delay_ms_(delay_ms), f_(f), state_(IDLE) {}

  void operator()(T x) {
    auto not_running = [this]() {
      mu_.AssertHeld();
      return state_ != RUNNING;
    };
    mu_.LockWhen(absl::Condition(&not_running));
    last_set_ = timestamp();
    last_value_ = x;
    if (state_ == IDLE) {
      state_ = SCHEDULED;
      std::thread([this]() {
        double time_sleeping;
        mu_.Lock();
        for (;;) {
          time_sleeping = timestamp() - last_set_;
          Log() << "time_sleeping:" << time_sleeping;
          if (time_sleeping >= delay_ms_ * 1e-3) {
            Log() << "time_for_work";
            break;
          }
          mu_.Unlock();
          usleep(delay_ms_ * 1e3 - time_sleeping * 1e6);
          mu_.Lock();
        }
        state_ = RUNNING;
        T x = last_value_;
        mu_.Unlock();

        Log() << "run";
        f_(x);
        Log() << "run done";

        mu_.Lock();
        state_ = IDLE;
        mu_.Unlock();

        Log() << "thread done";
      }).detach();
    }
    mu_.Unlock();
  }

 private:
  const int delay_ms_;
  const std::function<void(T)> f_;

  static double timestamp() {
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + 1e-6 * now.tv_usec;
  }

  enum State { IDLE, SCHEDULED, RUNNING };

  absl::Mutex mu_;
  double last_set_ GUARDED_BY(mu_);
  T last_value_ GUARDED_BY(mu_);
  State state_ GUARDED_BY(mu_);
};
