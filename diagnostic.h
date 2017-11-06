#pragma once

#include "crdt.h"
#include "woot.h"

class Diagnostic;
typedef std::shared_ptr<const Diagnostic> DiagnosticPtr;

class EditResponse;

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
    return (index < other.index) || (index == other.index && severity < other.severity) || (index == other.index && severity == other.severity && message < other.message);
  }
};

struct Fixit {
  size_t index;
  ID diagnostic;
  String::CommandBuf commands;
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
