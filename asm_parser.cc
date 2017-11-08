#include "asm_parser.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"
#include "log.h"
#include "re2/re2.h"

static bool isws(char c) {
  switch (c) {
    case ' ': case '\r': case '\n': case '\t': return true;
    default: return false;
  }
}

static absl::string_view StripRight(absl::string_view in) {
  while (!in.empty() && isws(in.back())) {
    in.remove_suffix(1);
  }
  return in;
}

AsmParseResult AsmParse(const std::string& src) {
  RE2 r_start{R"(Disassembly of section \.text:.*)"};
  RE2 r_section_start{R"([0-9a-f]+\s+<([^>]+)>:.*)"};

  AsmParseResult r;

  enum State {
    INIT,
    BODY,
  };

  auto emit = [&r](absl::string_view out) {
    r.body.append(out.begin(), out.end());
    r.body += '\n';
  };

  std::string label;

  State state = INIT;
  for (auto line : absl::StrSplit(src, '\n')) {
    line = StripRight(line);
    if (line.empty()) continue;

    re2::StringPiece spline(line.data(), line.length());

    switch (state) {
    case INIT:
      if (RE2::FullMatch(spline, r_start)) {
        state = BODY;
      }
      break;
    case BODY:
      if (RE2::FullMatch(spline, r_section_start, &label)) {
        emit(absl::StrCat(label, ":"));
        break;
      }
      emit(line);
      break;
    }
  }

  return r;
}

