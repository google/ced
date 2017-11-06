#include "libclang_collaborator.h"
#include <unordered_map>
#include "clang-c/Index.h"
#include "log.h"
#include "token_type.h"

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
      token_editor_(site()) {}

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

EditResponse LibClangCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;

  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  auto str = notification.content.Render();
  env->UpdateUnsavedFile(buffer_->filename(), str);
  std::vector<const char*> cmd_args;
  std::vector<CXUnsavedFile> unsaved_files = env->GetUnsavedFiles();
  const int options = 0;
  CXTranslationUnit tu = clang_parseTranslationUnit(
      env->index(), buffer_->filename().c_str(), cmd_args.data(),
      cmd_args.size(), unsaved_files.data(), unsaved_files.size(), options);
  if (tu == NULL) {
    Log() << "Cannot parse translation unit";
  }
  Log() << "Parsed: " << tu;

  CXFile file = clang_getFile(tu, buffer_->filename().c_str());

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

  unsigned ofs = 0;
  String::Iterator it(notification.content, String::Begin());
  it.MoveNext();
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

    while (ofs < offset_start) {
      it.MoveNext();
      ofs++;
    }
    ID id_start = it.id();
    while (ofs < offset_end) {
      it.MoveNext();
      ofs++;
    }
    ID id_end = it.id();

    token_editor_.Add(id_start, Annotation<Token>(id_end, KindToToken(kind)));
  }

  clang_disposeTokens(tu, tokens, numTokens);

  // fetch diagnostics
  if (notification.fully_loaded) {
  unsigned num_diagnostics = clang_getNumDiagnostics(tu);
  Log() << num_diagnostics << " diagnostics";
  for (unsigned i = 0; i < num_diagnostics; i++) {
    CXDiagnostic diag = clang_getDiagnostic(tu, i);
    Log() << clang_getCString(clang_formatDiagnostic(diag, 0));
    clang_disposeDiagnostic(diag);
  }
  }

  clang_disposeTranslationUnit(tu);

  token_editor_.Publish(&response);
  return response;
}
