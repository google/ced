#include "diagnostic.h"
#include "buffer.h"
#include "umap.h"
#include "uset.h"

namespace {

struct ReplacementData {
  std::pair<ID, ID> range;
  std::string new_text;
};

struct FixitData {
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
  Impl(Site* site) : diagnostic_editor(site), range_editor(site) {}
  std::vector<DiagData> new_diags;

  USetEditor<Diagnostic> diagnostic_editor;
  UMapEditor<ID, Annotation<ID>> range_editor;
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

DiagnosticEditor& DiagnosticEditor::StartFixit() {
  impl_->new_diags.back().fixits.emplace_back();
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

  impl_->new_diags.clear();
}
