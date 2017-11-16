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
#include "diagnostic.h"
#include "absl/strings/str_join.h"
#include "buffer.h"
#include "umap.h"
#include "uset.h"

namespace {

struct ReplacementData {
  std::pair<ID, ID> range;
  std::string new_text;
};

struct FixitData {
  Fixit::Type type;
  std::vector<ReplacementData> replacements;
};

struct DiagData {
  Severity severity;
  std::string message;
  ID diag_id;

  std::vector<std::pair<ID, ID>> ranges;
  std::vector<ID> points;
  std::vector<FixitData> fixits;
};

}  // namespace

struct DiagnosticEditor::Impl {
  Impl(Site* site)
      : site_(site),
        diagnostic_editor(site),
        range_editor(site),
        fixit_editor(site) {}
  Site* const site_;

  std::vector<DiagData> new_diags;

  USetEditor<Diagnostic> diagnostic_editor;
  UMapEditor<ID, Annotation<ID>> range_editor;
  USetEditor<Fixit> fixit_editor;
};

DiagnosticEditor::DiagnosticEditor(Site* site) : impl_(new Impl(site)) {}

DiagnosticEditor::~DiagnosticEditor() {}

DiagnosticEditor& DiagnosticEditor::StartDiagnostic(
    Severity severity, const std::string& message) {
  impl_->new_diags.emplace_back();
  impl_->new_diags.back().severity = severity;
  impl_->new_diags.back().message = message;
  return *this;
}

DiagnosticEditor& DiagnosticEditor::AddRange(ID ofs_begin, ID ofs_end) {
  impl_->new_diags.back().ranges.push_back(std::make_pair(ofs_begin, ofs_end));
  return *this;
}

DiagnosticEditor& DiagnosticEditor::AddPoint(ID point) {
  impl_->new_diags.back().points.push_back(point);
  return *this;
}

DiagnosticEditor& DiagnosticEditor::StartFixit(Fixit::Type type) {
  impl_->new_diags.back().fixits.emplace_back();
  impl_->new_diags.back().fixits.back().type = type;
  return *this;
}

DiagnosticEditor& DiagnosticEditor::AddReplacement(
    ID del_begin, ID del_end, const std::string& replacement) {
  impl_->new_diags.back().fixits.back().replacements.emplace_back(
      ReplacementData{{del_begin, del_end}, replacement});
  return *this;
}

void DiagnosticEditor::Publish(const String& content, EditResponse* response) {
  impl_->diagnostic_editor.BeginEdit(&response->diagnostics);
  for (size_t i = 0; i < impl_->new_diags.size(); i++) {
    Log() << "Diagnostic: " << i
          << " sev=" << static_cast<int>(impl_->new_diags[i].severity)
          << " message=" << impl_->new_diags[i].message;
    impl_->new_diags[i].diag_id = impl_->diagnostic_editor.Add(
        {i, impl_->new_diags[i].severity, impl_->new_diags[i].message});
  }
  impl_->diagnostic_editor.Publish();

  impl_->range_editor.BeginEdit(&response->diagnostic_ranges);
  for (const auto& diag : impl_->new_diags) {
    for (const auto& range : diag.ranges) {
      impl_->range_editor.Add(range.first,
                              Annotation<ID>(range.second, diag.diag_id));
    }
  }
  impl_->range_editor.Publish();

  impl_->fixit_editor.BeginEdit(&response->fixits);
  for (const auto& diag : impl_->new_diags) {
    size_t idx = 0;
    for (const auto& fixit : diag.fixits) {
      idx++;
      for (const auto& replacement : fixit.replacements) {
        ID id = impl_->fixit_editor.Add(
            Fixit{fixit.type, diag.diag_id, idx, replacement.range.first,
                  replacement.range.second, replacement.new_text});
        Log() << "PUBLISH FIXIT: " << absl::StrJoin(id, ":");
      }
    }
  }
  impl_->fixit_editor.Publish();

  impl_->new_diags.clear();
}
