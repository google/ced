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

#include "crdt.h"
#include "woot.h"

struct Diagnostic;
typedef std::shared_ptr<const Diagnostic> DiagnosticPtr;

struct EditResponse;

enum class Severity {
  UNSET,
  IGNORED,
  NOTE,
  WARNING,
  ERROR,
  FATAL,
};

// diagnostic annotations point to this
// as do fixit annotations
struct Diagnostic {
  size_t index;
  Severity severity;
  std::string message;

  bool operator<(const Diagnostic& other) const {
    return (index < other.index) ||
           (index == other.index && severity < other.severity) ||
           (index == other.index && severity == other.severity &&
            message < other.message);
  }
};

struct Fixit {
  size_t index;
  ID begin;
  ID end;
  std::string replacement;

  bool operator<(const Fixit& other) const {
    if (index < other.index) {
      return true;
    } else if (index > other.index) {
      return false;
    } else if (begin < other.begin) {
      return true;
    } else if (begin > other.begin) {
      return false;
    } else if (end < other.end) {
      return true;
    } else if (end > other.end) {
      return false;
    }
    return replacement < other.replacement;
  }
};

class DiagnosticEditor {
 public:
  DiagnosticEditor(Site* site);
  ~DiagnosticEditor();

  DiagnosticEditor& StartDiagnostic(Severity severity,
                                    const std::string& message);
  DiagnosticEditor& AddRange(ID ofs_begin, ID ofs_end);
  DiagnosticEditor& AddPoint(ID ofs);
  DiagnosticEditor& StartFixit();
  DiagnosticEditor& AddReplacement(ID del_begin, ID del_end,
                                   const std::string& replacement);

  void Publish(const String& content, EditResponse* response);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
