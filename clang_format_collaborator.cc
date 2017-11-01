#include "clang_format_collaborator.h"
#include "log.h"
#include "src/pugixml.hpp"
#include "subprocess.hpp"

using namespace subprocess;

void ClangFormatCollaborator::Push(const EditNotification& notification) {
  auto text = notification.content.Render();
  auto p = Popen({"clang-format", "-output-replacements-xml"}, input{PIPE},
                 output{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();

  pugi::xml_document doc;
  auto parse_result = doc.load_buffer_inplace(
      res.first.buf.data(), res.first.buf.size(),
      (pugi::parse_default | pugi::parse_ws_pcdata_single));

  if (!parse_result) return;

  struct Replacement {
    int offset;
    int length;
    const char* text;
  };
  std::vector<Replacement> replacements;

  for (auto replacement : doc.child("replacements").children("replacement")) {
    replacements.push_back(Replacement{replacement.attribute("offset").as_int(),
                                       replacement.attribute("length").as_int(),
                                       replacement.child_value()});
  }

  absl::MutexLock lock(&mu_);
  int n = 0;
  String::Iterator it(notification.content, String::Begin());
  it.MoveNext();
  for (auto r : replacements) {
    Log() << "REPLACE: " << r.offset << "+" << r.length << " with '" << r.text
          << "' starting from " << n;
    for (; n < r.offset; n++) {
      it.MoveNext();
    }
    auto after = it.id();
    for (int i = 0; i < r.length; i++) {
      auto del = it.id();
      it.MoveNext();
      n++;
      commands_.emplace_back(notification.content.MakeRemove(del));
    }
    for (const char* p = r.text; *p; ++p) {
      auto cmd = notification.content.MakeInsert(site(), *p, after);
      after = cmd->id();
      commands_.emplace_back(std::move(cmd));
    }
  }
}

EditResponse ClangFormatCollaborator::Pull() {
  auto ready = [this]() {
    mu_.AssertHeld();
    return shutdown_ || !commands_.empty();
  };
  EditResponse r;
  mu_.LockWhen(absl::Condition(&ready));
  r.commands.swap(commands_);
  r.done = shutdown_;
  mu_.Unlock();
  return r;
}

void ClangFormatCollaborator::Shutdown() {
  absl::MutexLock lock(&mu_);
  shutdown_ = true;
}
