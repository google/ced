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
#include <memory>
#include <unordered_map>
#include "absl/strings/str_join.h"
#include "buffer.h"
#include "clang-c/Index.h"
#include "clang_config.h"
#include "content_latch.h"
#include "libclang/libclang.h"
#include "log.h"
#include "selector.h"

class LibClangCollaborator final : public SyncCollaborator {
 public:
  LibClangCollaborator(const Buffer* buffer);
  ~LibClangCollaborator();

  EditResponse Edit(const EditNotification& notification) override;

 private:
  const Buffer* const buffer_;
  ContentLatch content_latch_;
  void* tu_ = nullptr;
  AnnotationEditor ed_;
};

namespace {

class ClangEnv : public LibClang, public ProjectAspect {
 public:
  ClangEnv(Project* project)
      : LibClang(ClangLibPath(project, "clang").c_str()) {
    if (!dlhdl) throw std::runtime_error("Failed opening libclang");
    index_ = this->clang_createIndex(1, 0);
  }

  ~ClangEnv() { clang_disposeIndex(index_); }

  absl::Mutex* mu() LOCK_RETURNED(mu_) { return &mu_; }

  void UpdateUnsavedFile(const boost::filesystem::path& filename,
                         const std::string& contents)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    unsaved_files_[absolute(filename).string()] = contents;
  }

  void ClearUnsavedFile(const boost::filesystem::path& filename)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    unsaved_files_.erase(absolute(filename).string());
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
  absl::Mutex mu_;
  CXIndex index_ GUARDED_BY(mu_);
  std::unordered_map<std::string, std::string> unsaved_files_ GUARDED_BY(mu_);
};

IMPL_PROJECT_GLOBAL_ASPECT(ClangEnv, project, 0) {
  if (project->client_peek()) return nullptr;
  try {
    return std::unique_ptr<ProjectAspect>(new ClangEnv(project));
  } catch (std::exception& e) {
    Log() << "Error starting clang environment: " << e.what();
    return nullptr;
  }
}

}  // namespace

LibClangCollaborator::LibClangCollaborator(const Buffer* buffer)
    : SyncCollaborator("libclang", absl::Seconds(0), absl::Milliseconds(1)),
      buffer_(buffer),
      content_latch_(true),
      ed_(buffer->site()) {}

LibClangCollaborator::~LibClangCollaborator() {
  ClangEnv* env = buffer_->project()->aspect<ClangEnv>();
  absl::MutexLock lock(env->mu());
  env->ClearUnsavedFile(buffer_->filename());

  if (tu_) {
    env->clang_disposeTranslationUnit(static_cast<CXTranslationUnit>(tu_));
  }
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

static Diagnostic::Severity DiagnosticSeverity(CXDiagnosticSeverity sev) {
  switch (sev) {
    case CXDiagnostic_Ignored:
      return Diagnostic::IGNORED;
    case CXDiagnostic_Note:
      return Diagnostic::NOTE;
    case CXDiagnostic_Warning:
      return Diagnostic::WARNING;
    case CXDiagnostic_Error:
      return Diagnostic::ERROR;
    case CXDiagnostic_Fatal:
      return Diagnostic::FATAL;
  }
  return Diagnostic::UNSET;
}

static bool IsPunctuation(char c) {
  switch (c) {
    case '\n':
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case '<':
    case '>':
    case '|':
    case '&':
    case '?':
    case '/':
    case ',':
    case '.':
    case '=':
    case '+':
    case '-':
    case '*':
    case '^':
    case '%':
    case '#':
    case '!':
    case '~':
    case '"':
    case '\t':
    case ' ':
      return true;
    default:
      return false;
  }
}

EditResponse LibClangCollaborator::Edit(const EditNotification& notification) {
  LogTimer tmr("libclang_edit");

  EditResponse response;

  bool content_changed = content_latch_.IsNewContent(notification);

  if (!content_changed) {
    return response;
  }

  auto filename = buffer_->filename();

  ClangEnv* env = buffer_->project()->aspect<ClangEnv>();

  std::string str;
  std::vector<ID> ids;
  AnnotatedString::Iterator it(notification.content, AnnotatedString::Begin());
  it.MoveNext();
  int line = 1;
  int col = 1;
  while (!it.is_end()) {
    if (it.value() == '\n') {
      line++;
      col = 0;
    } else {
      col++;
    }
    str += it.value();
    ids.push_back(it.id());
    it.MoveNext();
  }

  tmr.Mark("prelude");

  absl::MutexLock lock(env->mu());
  env->UpdateUnsavedFile(filename, str);
  std::vector<std::string> cmd_args_strs;
  ClangCompileArgs(buffer_->project(), filename, &cmd_args_strs);
  std::vector<const char*> cmd_args;
  for (auto& arg : cmd_args_strs) {
    cmd_args.push_back(arg.c_str());
  }
  Log() << "libclang args: " << absl::StrJoin(cmd_args, " ");
  std::vector<CXUnsavedFile> unsaved_files = env->GetUnsavedFiles();
  CXTranslationUnit tu = static_cast<CXTranslationUnit>(tu_);
  if (tu == nullptr) {
    const int options = env->clang_defaultEditingTranslationUnitOptions() |
                        CXTranslationUnit_KeepGoing |
                        CXTranslationUnit_DetailedPreprocessingRecord |
                        CXTranslationUnit_PrecompiledPreamble;
    tu = env->clang_parseTranslationUnit(
        env->index(), filename.c_str(), cmd_args.data(), cmd_args.size(),
        unsaved_files.data(), unsaved_files.size(), options);
    if (tu == NULL) {
      Log() << "Cannot parse translation unit";
      return response;
    }
    Log() << "Parsed: " << tu;

    tmr.Mark("parse");

    tu_ = tu;
    content_changed = true;
  }

  if (0 != env->clang_reparseTranslationUnit(
               tu, unsaved_files.size(), unsaved_files.data(),
               env->clang_defaultReparseOptions(tu))) {
    Log() << "failed reparse";
    return response;
  }

  tmr.Mark("reparse");

  {
    AnnotationEditor::ScopedEdit edit(&ed_, &response.content_updates);

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

    std::function<void(TagSet*, CXCursor)> f_add =
        [&f_add, env](TagSet* t, CXCursor cursor) {
          if (!env->clang_Cursor_isNull(cursor)) {
            f_add(t, env->clang_getCursorLexicalParent(cursor));
          }
          CXCursorKind kind = env->clang_getCursorKind(cursor);
          auto it = tok_cursor_rules.find(kind);
          if (it != tok_cursor_rules.end()) {
            t->add_tags(it->second);
          }
          t->add_tags(absl::StrCat(
              "LIBCLANG-",
              env->clang_getCString(env->clang_getCursorKindSpelling(kind))));
        };
    std::function<void(TagSet*, CXToken)> f_tidy = [env](TagSet* t,
                                                         CXToken token) {
      switch (env->clang_getTokenKind(token)) {
        case CXToken_Keyword:
          t->add_tags("keyword.c++");
          break;
        case CXToken_Comment:
          t->add_tags("comment.c++");
          break;
        default:
          break;
      }
    };

    std::map<unsigned, long long> ofs_annotation;

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

      if (ofs_annotation.find(line) == ofs_annotation.end()) {
        long long ofs = env->clang_Cursor_getOffsetOfField(cursor);
        if (ofs >= 0) {
          ofs_annotation[line] = ofs;
          Attribute attr;
          SizeAnnotation* ann = attr.mutable_size();
          ann->set_type(SizeAnnotation::OFFSET_INTO_PARENT);
          ann->set_size(ofs / 8);
          ann->set_bits(ofs % 8);
          ed_.Mark(ids[offset_start], ids[offset_end], attr);
        }
      }

      Attribute attr;
      TagSet* ts = attr.mutable_tags();
      ts->add_tags("source.c++");
      f_add(ts, cursor);
      f_tidy(ts, token);
      ed_.Mark(ids[offset_start], ids[offset_end], attr);
    }

    env->clang_disposeTokens(tu, tokens, numTokens);

    tmr.Mark("syntax-highlighting");

    /*
     * REFERENCED FILE DISCOVERY
     */

    env->clang_visitChildren(
        env->clang_getTranslationUnitCursor(tu),
        +[](CXCursor cursor, CXCursor parent, CXClientData client_data) {
          LibClangCollaborator* self =
              static_cast<LibClangCollaborator*>(client_data);
          ClangEnv* env = self->buffer_->project()->aspect<ClangEnv>();
          if (env->clang_getCursorKind(cursor) == CXCursor_InclusionDirective) {
            CXFile file = env->clang_getIncludedFile(cursor);
            if (file) {
              CXString filename = env->clang_getFileName(file);
              const char* fn = env->clang_getCString(filename);
              Attribute attr;
              attr.mutable_dependency()->set_filename(fn);
              self->ed_.AttrID(attr);
              env->clang_disposeString(filename);
            }
          }
          return CXChildVisit_Recurse;
        },
        this);

    tmr.Mark("referenced-files");

    /*
     * DIAGNOSTIC DISPLAY
     */

    if (notification.fully_loaded) {
      unsigned num_diagnostics = env->clang_getNumDiagnostics(tu);
      Log() << num_diagnostics << " diagnostics";
      for (unsigned i = 0; i < num_diagnostics; i++) {
        CXDiagnostic cxdiag = env->clang_getDiagnostic(tu, i);
        CXString message = env->clang_formatDiagnostic(cxdiag, 0);
        Attribute diag_attr;
        Diagnostic* diag = diag_attr.mutable_diagnostic();
        diag->set_severity(
            DiagnosticSeverity(env->clang_getDiagnosticSeverity(cxdiag)));
        diag->set_message(env->clang_getCString(message));
        ID diag_id = ed_.AttrID(diag_attr);
        unsigned num_ranges = env->clang_getDiagnosticNumRanges(cxdiag);
        for (size_t j = 0; j < num_ranges; j++) {
          CXSourceRange extent = env->clang_getDiagnosticRange(cxdiag, j);
          CXSourceLocation start = env->clang_getRangeStart(extent);
          CXSourceLocation end = env->clang_getRangeEnd(extent);
          CXFile file;
          unsigned line, col, offset_start, offset_end;
          env->clang_getFileLocation(start, &file, &line, &col, &offset_start);
          env->clang_getFileLocation(end, &file, &line, &col, &offset_end);
          if (file && boost::filesystem::equivalent(
                          filename, env->clang_getCString(
                                        env->clang_getFileName(file)))) {
            ed_.Mark(ids[offset_start], ids[offset_end], diag_id);
          }
        }
        CXFile file;
        unsigned line, col, offset;
        CXSourceLocation loc = env->clang_getDiagnosticLocation(cxdiag);
        env->clang_getFileLocation(loc, &file, &line, &col, &offset);
        if (file &&
            filename == env->clang_getCString(env->clang_getFileName(file))) {
          // diagnostic_editor_.AddPoint(ids[offset]);
        }
        unsigned num_fixits = env->clang_getDiagnosticNumFixIts(cxdiag);
        Log() << "num_fixits:" << num_fixits;
        for (unsigned j = 0; j < num_fixits; j++) {
          CXSourceRange extent;
          CXString repl = env->clang_getDiagnosticFixIt(cxdiag, j, &extent);
          CXSourceLocation start = env->clang_getRangeStart(extent);
          CXSourceLocation end = env->clang_getRangeEnd(extent);
          CXFile file;
          unsigned line, col, offset_start, offset_end;
          env->clang_getFileLocation(start, &file, &line, &col, &offset_start);
          env->clang_getFileLocation(end, &file, &line, &col, &offset_end);
          Log() << file;
          if (file)
            Log() << filename << " "
                  << env->clang_getCString(env->clang_getFileName(file));
          if (file && boost::filesystem::equivalent(
                          filename, env->clang_getCString(
                                        env->clang_getFileName(file)))) {
            Attribute fix_attr;
            Fixit* fix = fix_attr.mutable_fixit();
            fix->set_type(Fixit::COMPILE_FIX);
            fix->set_diagnostic(diag_id.id);
            fix->set_replacement(env->clang_getCString(repl));
            ed_.Mark(ids[offset_start], ids[offset_end], fix_attr);
          }
        }

        env->clang_disposeString(message);
        env->clang_disposeDiagnostic(cxdiag);
      }
    }

    tmr.Mark("diagnostics");
  }
  return response;
}

SERVER_COLLABORATOR(LibClangCollaborator, buffer) {
  ClangEnv* env = buffer->project()->aspect<ClangEnv>();
  if (env == nullptr) return false;
  auto fext = buffer->filename().extension();
  for (auto mext :
       {".c", ".cxx", ".cpp", ".C", ".cc", ".h", ".H", ".hpp", ".hxx"}) {
    if (fext == mext) return true;
  }
  return false;
}
