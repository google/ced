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
#include "absl/strings/str_cat.h"
#include "buffer.h"
#include "clang_config.h"
#include "log.h"
#include "run.h"
#include "src/pugixml.hpp"

class ClangFormatCollaborator final : public SyncCollaborator {
 public:
  ClangFormatCollaborator(const Buffer* buffer)
      : SyncCollaborator("clang-format", absl::Seconds(2),
                         absl::Milliseconds(100)),
        buffer_(buffer) {}

  EditResponse Edit(const EditNotification& notification) override;

 private:
  const Buffer* const buffer_;
};

EditResponse ClangFormatCollaborator::Edit(
    const EditNotification& notification) {
  EditResponse response;
  if (!notification.fully_loaded) return response;
  auto str = notification.content;
  auto text = str.Render();
  auto clang_format = ClangToolPath(buffer_->project(), "clang-format");
  Log() << "clang-format command: " << clang_format;
  auto res =
      run(clang_format,
          {"-output-replacements-xml",
           absl::StrCat("-assume-filename=", buffer_->filename().string())},
          text);
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
  AnnotatedString::Iterator it(str, AnnotatedString::Begin());
  it.MoveNext();
  for (auto r : replacements) {
    Log() << "REPLACE: " << r.offset << "+" << r.length << " with '" << r.text
          << "' starting from " << n;
    for (; n < r.offset; n++) {
      it.MoveNext();
    }
    for (int i = 0; i < r.length; i++) {
      auto del = it.id();
      it.MoveNext();
      n++;
      str.MakeDelete(&response.content_updates, del);
    }
    if (r.text[0]) {
      str.MakeInsert(&response.content_updates, buffer_->site(), r.text,
                     it.Prev().id());
    }
  }

  return response;
}

SERVER_COLLABORATOR(ClangFormatCollaborator, buffer) {
  auto fext = buffer->filename().extension();
  for (auto mext : {".c", ".cxx", ".cpp", ".C", ".cc", ".h", ".H", ".hpp",
                    ".hxx", ".proto", ".js", ".java", ".m"}) {
    if (fext == mext) return true;
  }
  return false;
}
