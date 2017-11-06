#pragma once

#include "crdt.h"
#include "woot.h"
#include "buffer.h"

class Diagnostic;
typedef std::shared_ptr<const Diagnostic> DiagnosticPtr;

enum class Severity {
  UNSET,
  IGNORED,
  NOTE,
  WARNING,
  ERROR,
  FATAL,
};

class Diagnostic {
 public:
};

class DiagnosticEditor {
 public:
  DiagnosticEditor();
  ~DiagnosticEditor();

  DiagnosticEditor& StartDiagnostic(Severity severity,
                                    const std::string& message);
  DiagnosticEditor& AddRange(size_t ofs_begin, size_t ofs_end);
  DiagnosticEditor& AddPoint(size_t ofs);
  DiagnosticEditor& StartFixit();
  DiagnosticEditor& AddReplacement(size_t del_begin, size_t del_end,
                                   const std::string& replacement);

  void FinishAndPublishChanges(EditResponse* response);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
