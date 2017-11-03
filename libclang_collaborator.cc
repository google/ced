#include "libclang_collaborator.h"
#include <unordered_map>
#include "clang-c/Index.h"
#include "log.h"

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

struct LibClangCollaborator::Impl {
  Impl(const Buffer* b) : buffer(b), shutdown(false) {}

  const Buffer* const buffer;
  CommandBuf commands GUARDED_BY(ClangEnv::Get()->mu());
  bool shutdown GUARDED_BY(ClangEnv::Get()->mu());
};

LibClangCollaborator::LibClangCollaborator(const Buffer* buffer)
    : Collaborator("libclang", absl::Seconds(0)), impl_(new Impl(buffer)) {}

LibClangCollaborator::~LibClangCollaborator() {
  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  env->ClearUnsavedFile(impl_->buffer->filename());
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

void LibClangCollaborator::Push(const EditNotification& notification) {
  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  auto str = notification.content.Render();
  env->UpdateUnsavedFile(impl_->buffer->filename(), str);
  std::vector<const char*> cmd_args;
  std::vector<CXUnsavedFile> unsaved_files = env->GetUnsavedFiles();
  const int options = 0;
  CXTranslationUnit tu = clang_parseTranslationUnit(
      env->index(), impl_->buffer->filename().c_str(), cmd_args.data(),
      cmd_args.size(), unsaved_files.data(), unsaved_files.size(), options);
  if (tu == NULL) {
    Log() << "Cannot parse translation unit";
  }
  Log() << "Parsed: " << tu;

  CXFile file = clang_getFile(tu, impl_->buffer->filename().c_str());

  // get top/last location of the file
  CXSourceLocation topLoc = clang_getLocationForOffset(tu, file, 0);
  CXSourceLocation lastLoc = clang_getLocationForOffset(tu, file, str.length());
  if (clang_equalLocations(topLoc, clang_getNullLocation()) ||
      clang_equalLocations(lastLoc, clang_getNullLocation())) {
    Log() << "cannot retrieve location";
    clang_disposeTranslationUnit(tu);
    return;
  }

  // make a range from locations
  CXSourceRange range = clang_getRange(topLoc, lastLoc);
  if (clang_Range_isNull(range)) {
    Log() << "cannot retrieve range";
    clang_disposeTranslationUnit(tu);
    return;
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

    Log() << "TOK:" << offset_start << ".." << offset_end << " -> " << kind;

    while (ofs < offset_start) {
      if (it.token_type() != Token::UNSET) {
        impl_->commands.emplace_back(
            notification.content.MakeSetTokenType(it.id(), Token::UNSET));
      }
      it.MoveNext();
      ofs++;
    }

    Token type = KindToToken(kind);
    while (ofs < offset_end) {
      if (it.token_type() != type) {
        impl_->commands.emplace_back(
            notification.content.MakeSetTokenType(it.id(), type));
      }
      it.MoveNext();
      ofs++;
    }
  }

  clang_disposeTokens(tu, tokens, numTokens);
  clang_disposeTranslationUnit(tu);
}

EditResponse LibClangCollaborator::Pull() {
  auto ready = [this]() {
    ClangEnv::Get()->mu()->AssertHeld();
    return !impl_->commands.empty() || impl_->shutdown;
  };
  
  ClangEnv* env = ClangEnv::Get();
  env->mu()->LockWhen(absl::Condition(&ready));
  EditResponse r;
  r.commands.swap(impl_->commands);
  r.done = impl_->shutdown;
  env->mu()->Unlock();
  return r;
}

void LibClangCollaborator::Shutdown() {
  ClangEnv* env = ClangEnv::Get();
  absl::MutexLock lock(env->mu());
  impl_->shutdown = true;
}
