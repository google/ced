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
#pragma once

#include <fcntl.h>
#include <gflags/gflags.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <sstream>
#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "wrap_syscall.h"

DECLARE_string(logfile);

class Log : public std::ostringstream {
 public:
  Log() : cerr_(log_cerr_.load()) {
    if (FLAGS_logfile.empty() && !cerr_) {
      this->setstate(std::ios_base::badbit);
    }
  }
  ~Log() {
    if (!cerr_ && FLAGS_logfile.empty()) return;
    *this << '\n';
#ifdef __APPLE__
    uint64_t thread_id;
    pthread_threadid_np(NULL, &thread_id);
#else
    int thread_id = -1;
#endif
    auto s = absl::StrCat("[", thread_id, "] ", absl::FormatTime(absl::Now()),
                          "  ", str());
    if (!FLAGS_logfile.empty()) {
      static LogFile file;
      WrapSyscall("write", [this, &s]() {
        return ::write(file.hdl(), s.data(), s.length());
      });
    }
    if (cerr_) {
      try {
        WrapSyscall("write", [this, &s]() {
          return ::write(STDERR_FILENO, s.data(), s.length());
        });
      } catch (...) {
      }
    }
  }

  Log(const Log&) = delete;
  Log& operator=(const Log&) = delete;

  static void SetCerrLog(bool x) { log_cerr_ = x; }

 private:
  class LogFile {
   public:
    LogFile() {
      hdl_ = WrapSyscall("open", []() {
        return open(FLAGS_logfile.c_str(),
                    O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, 0777);
      });
    }

    ~LogFile() { close(hdl_); }

    int hdl() const { return hdl_; }

   private:
    int hdl_;
  };

  static std::atomic<bool> log_cerr_;
  const bool cerr_;
};

class LogTimer {
 public:
  LogTimer(const char* pfx) : start_(absl::Now()), pfx_(pfx) {}

  void Mark(const char* ent) {
    marks_.push_back(std::make_pair(ent, absl::Now()));
  }

  ~LogTimer() {
    auto end = absl::Now();
    std::string report;
    absl::StrAppend(&report, "Completed ", pfx_, " in ",
                    absl::FormatDuration(end - start_));
    auto last = start_;
    for (const auto& m : marks_) {
      absl::StrAppend(&report, "\n  ", m.first, " in ",
                      absl::FormatDuration(m.second - last));
      last = m.second;
    }
    Log() << report;
  }

 private:
  absl::Time start_;
  const char* const pfx_;
  std::vector<std::pair<const char*, absl::Time>> marks_;
};
