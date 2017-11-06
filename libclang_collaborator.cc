#include "libclang_collaborator.h"
#include <unordered_map>
#include "clang-c/Index.h"
#include "log.h"
#include "token_type.h"
#include "diagnostic.h"

namespace {

class ClangEnv {
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

  CXIndex index() const { return index_; }

 private:
  ClangEnv() { index_ = clang_createIndex(1, 0); }

  absl::Mutex mu_;
  CXIndex index_ GUARDED_BY(mu_);
  std::unordered_map<std::string, std::string> unsaved_files_ GUARDED_BY(mu_);
};

}  // namespace

LibClangCollaborator::LibClangCollaborator(const Buffer* buffer)
    : SyncCollaborator("libclang", absl::Seconds(0)),
      buffer_(buffer),
      token_editor_(site()), diagnostic_editor_(site()) {}

LibClangCollaborator::~LibClangCollaborator() {
  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  env->ClearUnsavedFile(buffer_->filename());
}

static Token KindToToken(CXTokenKind kind) {
  switch (kind) {
    case CXToken_Punctuation:
      return Token::SYMBOL;
    case CXToken_Keyword:
      return Token::KEYWORD;
    case CXToken_Identifier:
      return Token::IDENT;
    case CXToken_Literal:
      return Token::LITERAL;
    case CXToken_Comment:
      return Token::COMMENT;
  }
  return Token::UNSET;
}

static Severity DiagnosticSeverity(CXDiagnosticSeverity sev) {
  switch (sev) {
    case CXDiagnostic_Ignored: return Severity::IGNORED;
    case CXDiagnostic_Note: return Severity::NOTE;
    case CXDiagnostic_Warning: return Severity::WARNING;
    case CXDiagnostic_Error: return Severity::ERROR;
    case CXDiagnostic_Fatal: return Severity::FATAL;
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
  std::vector<const char*> cmd_args;
  std::vector<CXUnsavedFile> unsaved_files = env->GetUnsavedFiles();
  const int options = 0;
  CXTranslationUnit tu = clang_parseTranslationUnit(
      env->index(), filename.c_str(), cmd_args.data(),
      cmd_args.size(), unsaved_files.data(), unsaved_files.size(), options);
  if (tu == NULL) {
    Log() << "Cannot parse translation unit";
  }
  Log() << "Parsed: " << tu;

  CXFile file = clang_getFile(tu, filename.c_str());

  // get top/last location of the file
  CXSourceLocation topLoc = clang_getLocationForOffset(tu, file, 0);
  CXSourceLocation lastLoc = clang_getLocationForOffset(tu, file, str.length());
  if (clang_equalLocations(topLoc, clang_getNullLocation()) ||
      clang_equalLocations(lastLoc, clang_getNullLocation())) {
    Log() << "cannot retrieve location";
    clang_disposeTranslationUnit(tu);
    return response;
  }

  // make a range from locations
  CXSourceRange range = clang_getRange(topLoc, lastLoc);
  if (clang_Range_isNull(range)) {
    Log() << "cannot retrieve range";
    clang_disposeTranslationUnit(tu);
    return response;
  }

  CXToken* tokens;
  unsigned numTokens;
  clang_tokenize(tu, range, &tokens, &numTokens);

  token_editor_.BeginEdit(&response.token_types);

  unsigned ofs = 0;
  for (unsigned i = 0; i < numTokens; i++) {
    CXToken token = tokens[i];
    CXSourceRange extent = clang_getTokenExtent(tu, token);
    CXSourceLocation start = clang_getRangeStart(extent);
    CXSourceLocation end = clang_getRangeEnd(extent);
    CXTokenKind kind = clang_getTokenKind(token);

    CXFile file;
    unsigned line, col, offset_start, offset_end;
    clang_getFileLocation(start, &file, &line, &col, &offset_start);
    clang_getFileLocation(end, &file, &line, &col, &offset_end);

    token_editor_.Add(ids[offset_start], Annotation<Token>(ids[offset_end], KindToToken(kind)));
  }

  clang_disposeTokens(tu, tokens, numTokens);

  // fetch diagnostics
  if (notification.fully_loaded) {
    unsigned num_diagnostics = clang_getNumDiagnostics(tu);
    Log() << num_diagnostics << " diagnostics";
    for (unsigned i = 0; i < num_diagnostics; i++) {
      CXDiagnostic diag = clang_getDiagnostic(tu, i);
      CXString message = clang_formatDiagnostic(diag, 0);
      diagnostic_editor_.StartDiagnostic(DiagnosticSeverity(clang_getDiagnosticSeverity(diag)), clang_getCString(message));
      unsigned num_ranges = clang_getDiagnosticNumRanges(diag);
      for (size_t j=0; j<num_ranges; j++) {
        CXSourceRange extent = clang_getDiagnosticRange(diag, j);
        CXSourceLocation start = clang_getRangeStart(extent);
        CXSourceLocation end = clang_getRangeEnd(extent);
        CXFile file;
        unsigned line, col, offset_start, offset_end;
        clang_getFileLocation(start, &file, &line, &col, &offset_start);
        clang_getFileLocation(end, &file, &line, &col, &offset_end);
        if (filename == clang_getCString(clang_getFileName(file))) {
          diagnostic_editor_.AddRange(ids[offset_start], ids[offset_end]);
        }
      }
      CXFile file;
      unsigned line, col, offset;
      CXSourceLocation loc = clang_getDiagnosticLocation(diag);
      clang_getFileLocation(loc, &file, &line, &col, &offset);
      if (filename == clang_getCString(clang_getFileName(file))) {
        diagnostic_editor_.AddPoint(ids[offset]);
      }
      unsigned num_fixits = clang_getDiagnosticNumFixIts(diag);
      for (unsigned j=0; j<num_fixits; j++) {
        CXSourceRange extent;
        CXString repl = clang_getDiagnosticFixIt(diag, j, &extent);
        CXSourceLocation start = clang_getRangeStart(extent);
        CXSourceLocation end = clang_getRangeEnd(extent);
        CXFile file;
        unsigned line, col, offset_start, offset_end;
        clang_getFileLocation(start, &file, &line, &col, &offset_start);
        clang_getFileLocation(end, &file, &line, &col, &offset_end);
        if (filename == clang_getCString(clang_getFileName(file))) {
          diagnostic_editor_.StartFixit().AddReplacement(ids[offset_start], ids[offset_end], clang_getCString(repl));
        }
      }

      //Log() << clang_getCString(clang_formatDiagnostic(diag, 0));
      clang_disposeString(message);
      clang_disposeDiagnostic(diag);
    }
  }

  clang_disposeTranslationUnit(tu);

  token_editor_.Publish();
  diagnostic_editor_.Publish(notification.content, &response);
  return response;
}
