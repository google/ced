#pragma once

#include "woot.h"

class Diagnostic;
typedef std::shared_ptr<const Diagnostic> DiagnosticPtr;

enum class Severity {
  INFO,
  WARNING,
  ERROR,
};

class Diagnostic {
 public:
  ID start() const { return start_; }
  ID finish() const { return finish_; }
  const std::string& message() const { return message_; }

  String Apply(String s) const {
    for (const auto& cmd : fixit_) {
      s = s.Integrate(cmd);
    }
    return s;
  }

 private:
  const std::string message_;
  const std::vector<std::pair<ID, ID>> ranges_;
  const std::vector<ID> points_;
  const std::vector<String::CommandBuf> fixits_;
};

class DiagnosticSetBuilder {
 public:
  DiagnosticSetBuilder& StartDiagnostic(Severity severity,
                                        const std::string& message);
  DiagnosticSetBuilder& AddRange(size_t ofs_begin, size_t ofs_end);
  DiagnosticSetBuilder& AddPoint(size_t ofs);
  DiagnosticSetBuilder& StartFixit();
  DiagnosticSetBuilder& AddReplacement(size_t del_begin, size_t del_end,
                                       const std::string& replacement);

  void FinishAndPublishChanges(String& s, std::vector<CommandPtr>& changes);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
