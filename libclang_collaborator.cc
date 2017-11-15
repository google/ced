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
#include "libclang_collaborator.h"
#include <unordered_map>
#include "absl/strings/str_join.h"
#include "clang-c/Index.h"
#include "clang_config.h"
#include "diagnostic.h"
#include "libclang/libclang.h"
#include "log.h"
#include "selector.h"

namespace {

class ClangEnv : public LibClang {
 public:
  static ClangEnv* Get() {
    static ClangEnv env;
    return &env;
  }

  ~ClangEnv() { clang_disposeIndex(index_); }

  absl::Mutex* mu() LOCK_RETURNED(mu_) { return &mu_; }

  void UpdateUnsavedFile(std::string filename, const std::string& contents)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    unsaved_files_[filename] = contents;
  }

  void ClearUnsavedFile(std::string filename) EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    unsaved_files_.erase(filename);
  }

  std::vector<CXUnsavedFile> GetUnsavedFiles() EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    std::vector<CXUnsavedFile> unsaved_files;
    for (auto& f : unsaved_files_) {
      CXUnsavedFile u;
      u.Filename = f.first.c_str();
      u.Contents = f.second.data();
      u.Length = f.second.length();
      unsaved_files.push_back(u);
    }
    return unsaved_files;
  }

  CXIndex index() const EXCLUSIVE_LOCKS_REQUIRED(mu_) { return index_; }

 private:
  ClangEnv() : LibClang(ClangLibPath("clang").c_str()) {
    if (!dlhdl) throw std::runtime_error("Failed opening libclang");
    index_ = this->clang_createIndex(1, 0);
  }

  absl::Mutex mu_;
  CXIndex index_ GUARDED_BY(mu_);
  std::unordered_map<std::string, std::string> unsaved_files_ GUARDED_BY(mu_);
};

}  // namespace

LibClangCollaborator::LibClangCollaborator(const Buffer* buffer)
    : SyncCollaborator("libclang", absl::Seconds(0)),
      buffer_(buffer),
      token_editor_(site()),
      diagnostic_editor_(site()),
      ref_editor_(site()) {}

LibClangCollaborator::~LibClangCollaborator() {
  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  env->ClearUnsavedFile(buffer_->filename());
}

static const std::map<CXCursorKind, std::string> tok_cursor_rules = {
    {CXCursor_StructDecl, "meta.class-struct-block.c++"},
    {CXCursor_UnionDecl, "meta.class-struct-block.c++"},
    {CXCursor_ClassDecl, "meta.class-struct-block.c++"},
    {CXCursor_EnumDecl, "meta.enum-block.c++"},
    {CXCursor_DeclRefExpr, "entity.name.c++"},
    {CXCursor_MemberRefExpr, "entity.name.c++"},
    {CXCursor_CXXMethod, "entity.name.function"},
    {CXCursor_IntegerLiteral, "constant.numeric"},
    {CXCursor_FloatingLiteral, "constant.numeric"},
    {CXCursor_ImaginaryLiteral, "constant.numeric"},
    {CXCursor_StringLiteral, "string"},
    {CXCursor_CharacterLiteral, "string"},
    {CXCursor_BinaryOperator, "keyword.operator.binary.c++"},
    {CXCursor_UnaryOperator, "keyword.operator.unary.c++"}};

static Severity DiagnosticSeverity(CXDiagnosticSeverity sev) {
  switch (sev) {
    case CXDiagnostic_Ignored:
      return Severity::IGNORED;
    case CXDiagnostic_Note:
      return Severity::NOTE;
    case CXDiagnostic_Warning:
      return Severity::WARNING;
    case CXDiagnostic_Error:
      return Severity::ERROR;
    case CXDiagnostic_Fatal:
      return Severity::FATAL;
  }
  return Severity::UNSET;
}

EditResponse LibClangCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  auto filename = buffer_->filename();

  ClangEnv* env = ClangEnv::Get();

  std::string str;
  std::vector<ID> ids;
  String::Iterator it(notification.content, String::Begin());
  it.MoveNext();
  while (!it.is_end()) {
    str += it.value();
    ids.push_back(it.id());
    it.MoveNext();
  }

  absl::MutexLock lock(env->mu());
  env->UpdateUnsavedFile(filename, str);
  std::vector<std::string> cmd_args_strs;
  ClangCompileArgs(filename, &cmd_args_strs);
  std::vector<const char*> cmd_args;
  for (auto& arg : cmd_args_strs) {
    cmd_args.push_back(arg.c_str());
  }
  Log() << "libclang args: " << absl::StrJoin(cmd_args, " ");
  std::vector<CXUnsavedFile> unsaved_files = env->GetUnsavedFiles();
  const int options = env->clang_defaultEditingTranslationUnitOptions() |
                      CXTranslationUnit_KeepGoing |
                      CXTranslationUnit_DetailedPreprocessingRecord;
  CXTranslationUnit tu = env->clang_parseTranslationUnit(
      env->index(), filename.c_str(), cmd_args.data(), cmd_args.size(),
      unsaved_files.data(), unsaved_files.size(), options);
  if (tu == NULL) {
    Log() << "Cannot parse translation unit";
  }
  Log() << "Parsed: " << tu;

  CXFile file = env->clang_getFile(tu, filename.c_str());

  // get top/last location of the file
  CXSourceLocation topLoc = env->clang_getLocationForOffset(tu, file, 0);
  CXSourceLocation lastLoc =
      env->clang_getLocationForOffset(tu, file, str.length());
  if (env->clang_equalLocations(topLoc, env->clang_getNullLocation()) ||
      env->clang_equalLocations(lastLoc, env->clang_getNullLocation())) {
    Log() << "cannot retrieve location";
    env->clang_disposeTranslationUnit(tu);
    return response;
  }

  // make a range from locations
  CXSourceRange range = env->clang_getRange(topLoc, lastLoc);
  if (env->clang_Range_isNull(range)) {
    Log() << "cannot retrieve range";
    env->clang_disposeTranslationUnit(tu);
    return response;
  }

  /*
   * SYNTAX HIGHLIGHTING DATA
   */

  CXToken* tokens;
  unsigned numTokens;
  env->clang_tokenize(tu, range, &tokens, &numTokens);
  std::unique_ptr<CXCursor[]> tok_cursors(new CXCursor[numTokens]);
  env->clang_annotateTokens(tu, tokens, numTokens, tok_cursors.get());

  token_editor_.BeginEdit(&response.token_types);

  std::function<Tag(Tag, CXCursor)> f_add = [&f_add, env](Tag t,
                                                          CXCursor cursor) {
    if (!env->clang_Cursor_isNull(cursor)) {
      t = f_add(t, env->clang_getCursorLexicalParent(cursor));
    }
    CXCursorKind kind = env->clang_getCursorKind(cursor);
    auto it = tok_cursor_rules.find(kind);
    if (it != tok_cursor_rules.end()) {
      t = t.Push(it->second);
    }
    t = t.Push(absl::StrCat(
        "LIBCLANG-",
        env->clang_getCString(env->clang_getCursorKindSpelling(kind))));
    return t;
  };
  std::function<Tag(Tag, CXToken)> f_tidy = [env](Tag t, CXToken token) {
    switch (env->clang_getTokenKind(token)) {
      case CXToken_Keyword:
        return t.Push("keyword.c++");
      case CXToken_Comment:
        return t.Push("comment.c++");
      default:
        return t;
    }
  };

  for (unsigned i = 0; i < numTokens; i++) {
    CXToken token = tokens[i];
    CXCursor cursor = tok_cursors[i];
    CXSourceRange extent = env->clang_getTokenExtent(tu, token);
    CXSourceLocation start = env->clang_getRangeStart(extent);
    CXSourceLocation end = env->clang_getRangeEnd(extent);

    CXFile file;
    unsigned line, col, offset_start, offset_end;
    env->clang_getFileLocation(start, &file, &line, &col, &offset_start);
    env->clang_getFileLocation(end, &file, &line, &col, &offset_end);

    token_editor_.Add(
        ids[offset_start],
        Annotation<Tag>(
            ids[offset_end],
            f_tidy(f_add(Tag().Push("source.c++"), cursor), token)));
  }

  env->clang_disposeTokens(tu, tokens, numTokens);

  /*
   * REFERENCED FILE DISCOVERY
   */

  ref_editor_.BeginEdit(&response.referenced_files);
  env->clang_visitChildren(
      env->clang_getTranslationUnitCursor(tu),
      +[](CXCursor cursor, CXCursor parent, CXClientData client_data) {
        ClangEnv* env = ClangEnv::Get();
        if (env->clang_getCursorKind(cursor) == CXCursor_InclusionDirective) {
          CXFile file = env->clang_getIncludedFile(cursor);
          CXString filename = env->clang_getFileName(file);
          const char* fn = env->clang_getCString(filename);
          LibClangCollaborator* self =
              static_cast<LibClangCollaborator*>(client_data);
          self->ref_editor_.Add(fn);
          env->clang_disposeString(filename);
        }
        return CXChildVisit_Recurse;
      },
      this);
  ref_editor_.Publish();

  /*
   * DIAGNOSTIC DISPLAY
   */
  if (notification.fully_loaded) {
    unsigned num_diagnostics = env->clang_getNumDiagnostics(tu);
    Log() << num_diagnostics << " diagnostics";
    for (unsigned i = 0; i < num_diagnostics; i++) {
      CXDiagnostic diag = env->clang_getDiagnostic(tu, i);
      CXString message = env->clang_formatDiagnostic(diag, 0);
      diagnostic_editor_.StartDiagnostic(
          DiagnosticSeverity(env->clang_getDiagnosticSeverity(diag)),
          env->clang_getCString(message));
      unsigned num_ranges = env->clang_getDiagnosticNumRanges(diag);
      for (size_t j = 0; j < num_ranges; j++) {
        CXSourceRange extent = env->clang_getDiagnosticRange(diag, j);
        CXSourceLocation start = env->clang_getRangeStart(extent);
        CXSourceLocation end = env->clang_getRangeEnd(extent);
        CXFile file;
        unsigned line, col, offset_start, offset_end;
        env->clang_getFileLocation(start, &file, &line, &col, &offset_start);
        env->clang_getFileLocation(end, &file, &line, &col, &offset_end);
        if (file &&
            filename == env->clang_getCString(env->clang_getFileName(file))) {
          diagnostic_editor_.AddRange(ids[offset_start], ids[offset_end]);
        }
      }
      CXFile file;
      unsigned line, col, offset;
      CXSourceLocation loc = env->clang_getDiagnosticLocation(diag);
      env->clang_getFileLocation(loc, &file, &line, &col, &offset);
      if (file &&
          filename == env->clang_getCString(env->clang_getFileName(file))) {
        diagnostic_editor_.AddPoint(ids[offset]);
      }
      unsigned num_fixits = env->clang_getDiagnosticNumFixIts(diag);
      Log() << "num_fixits:" << num_fixits;
      for (unsigned j = 0; j < num_fixits; j++) {
        CXSourceRange extent;
        CXString repl = env->clang_getDiagnosticFixIt(diag, j, &extent);
        CXSourceLocation start = env->clang_getRangeStart(extent);
        CXSourceLocation end = env->clang_getRangeEnd(extent);
        CXFile file;
        unsigned line, col, offset_start, offset_end;
        env->clang_getFileLocation(start, &file, &line, &col, &offset_start);
        env->clang_getFileLocation(end, &file, &line, &col, &offset_end);
        if (file &&
            filename == env->clang_getCString(env->clang_getFileName(file))) {
          diagnostic_editor_.StartFixit().AddReplacement(
              ids[offset_start], ids[offset_end], env->clang_getCString(repl));
        }
      }

      env->clang_disposeString(message);
      env->clang_disposeDiagnostic(diag);
    }
  }

  env->clang_disposeTranslationUnit(tu);

  token_editor_.Publish();
  diagnostic_editor_.Publish(notification.content, &response);
  return response;
}
