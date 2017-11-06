#include "clang_format_collaborator.h"
#include "clang_config.h"
#include "log.h"
#include "src/pugixml.hpp"
#include "subprocess.hpp"

using namespace subprocess;

EditResponse ClangFormatCollaborator::Edit(
    const EditNotification& notification) {
  EditResponse response;
  if (!notification.fully_loaded) return response;
  auto str = notification.content;
  auto text = str.Render();
  auto clang_format = ClangToolPath("clang-format");
  Log() << "clang-format command: " << clang_format;
  auto p = Popen({clang_format.c_str(), "-output-replacements-xml"},
                 input{PIPE}, output{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();

  Log() << res.first.buf.data();

  pugi::xml_document doc;
  auto parse_result = doc.load_buffer_inplace(
      res.first.buf.data(), res.first.buf.size(),
      (pugi::parse_default | pugi::parse_ws_pcdata_single));

  if (!parse_result) {
    return response;
  }
  if (doc.child("replacements").attribute("incomplete_format").as_bool()) {
    return response;
  }

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

  int n = 0;
  String::Iterator it(str, String::Begin());
  it.MoveNext();
  for (auto r : replacements) {
    Log() << "REPLACE: " << r.offset << "+" << r.length << " with '" << r.text
          << "' starting from " << n;
    for (; n < r.offset; n++) {
      it.MoveNext();
    }
    auto refresh_it = [&]() {
      ID curid = it.id();
      str = str.Integrate(response.content.back());
      it = String::Iterator(str, curid);
    };
    for (int i = 0; i < r.length; i++) {
      auto del = it.id();
      it.MoveNext();
      n++;
      str.MakeRemove(&response.content, del);
      refresh_it();
    }
    auto after = it.Prev().id();
    for (const char* p = r.text; *p; ++p) {
      after = str.MakeInsert(&response.content, site(), *p, after);
      refresh_it();
    }
  }

  return response;
}
