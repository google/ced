#include "clang_format_collaborator.h"
#include "log.h"
#include "subprocess.hpp"

using namespace subprocess;

void ClangFormatCollaborator::Push(const EditNotification &notification) {
  auto text = notification.content.Render();
  auto p = Popen({"clang-format", "-output-replacements-xml"}, input{PIPE},
                 output{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();

  Log() << res.first.buf.data();
}

EditResponse ClangFormatCollaborator::Pull() {
  EditResponse r;
  r.done = true;
  return r;
}

void ClangFormatCollaborator::Shutdown() {
  absl::MutexLock lock(&mu_);
  shutdown_ = true;
}
