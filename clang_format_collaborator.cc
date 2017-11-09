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
#include "clang_format_collaborator.h"
#include "clang_config.h"
#include "log.h"
#include "run.h"
#include "src/pugixml.hpp"
#include "absl/strings/str_cat.h"

EditResponse ClangFormatCollaborator::Edit(
    const EditNotification& notification) {
  EditResponse response;
  if (!notification.fully_loaded) return response;
  auto str = notification.content;
  auto text = str.Render();
  auto clang_format = ClangToolPath("clang-format");
  Log() << "clang-format command: " << clang_format;
  auto res = run(clang_format, {"-output-replacements-xml", absl::StrCat("-assume-filename=", buffer_->filename())}, text);
  Log() << res.out;

  pugi::xml_document doc;
  auto parse_result =
      doc.load_buffer(res.out.data(), res.out.length(),
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
