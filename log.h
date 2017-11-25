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
#include <unistd.h>
#include <sstream>
#include <vector>
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "wrap_syscall.h"
#include <gflags/gflags.h>

DECLARE_string(logfile);

class Log : public std::ostringstream {
 public:
  Log() = default;
  ~Log() {
    if (!FLAGS_logfile.empty()) {
      static LogFile file;
      *this << '\n';
      WrapSyscall("write", [this]() {
        auto s = str();
        return ::write(file.hdl(), s.data(), s.length());
      });
    }
  }

  Log(const Log&) = delete;
  Log& operator=(const Log&) = delete;

 private:
  class LogFile {
   public:
    LogFile() {
      hdl_ = WrapSyscall("open", []() {
        return open(FLAGS_logfile.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0777);
      });
    }

    ~LogFile() { close(hdl_); }

    int hdl() const { return hdl_; }

   private:
    int hdl_;
  };
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
