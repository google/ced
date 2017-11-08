#include "asm_parser.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cppfilt.h"
#include "log.h"
#include "re2/re2.h"

static bool isws(char c) {
  switch (c) {
    case ' ':
    case '\r':
    case '\n':
    case '\t':
      return true;
    default:
      return false;
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
  RE2 r_label{R"(([^ ()]+)([()]*):[^0-9]*)"};
  RE2 r_lineno{R"(([^ ]+):([0-9]+))"};
  RE2 r_has_stdin{R"(.*<stdin>.*)"};
  RE2 r_instr{R"(\s+[0-9a-f]+:\s*(.*))"};

  AsmParseResult r;

  enum State {
    INIT,
    BODY,
  };

  int num_lines = 0;
  auto emit = [&r, &num_lines](absl::string_view out) {
    r.body.append(out.begin(), out.end());
    r.body += '\n';
    num_lines++;
  };

  std::string label;
  int src_line;

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
          break;
        }
        if (RE2::FullMatch(spline, r_label, &label)) {
          emit(absl::StrCat(cppfilt(label), ":"));
          break;
        }
        if (RE2::FullMatch(spline, r_lineno, &label, &src_line)) {
          if (RE2::FullMatch(label, r_has_stdin)) {
            r.src_to_asm_line[src_line].push_back(num_lines);
          }
          break;
        }
        if (RE2::FullMatch(spline, r_instr, &label)) {
          emit(absl::StrCat("  ", label));
          break;
        }
        emit(line);
        break;
    }
  }

  return r;
}
