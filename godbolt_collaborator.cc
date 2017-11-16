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
#include "godbolt_collaborator.h"
#include "absl/strings/str_join.h"
#include "asm_parser.h"
#include "clang_config.h"
#include "log.h"
#include "run.h"
#include "temp_file.h"

#if defined(__APPLE__)
#define OBJDUMP_BIN "gobjdump"
#else
#define OBJDUMP_BIN "objdump"
#endif

EditResponse GodboltCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  response.done = notification.shutdown;
  if (response.done) return response;
  if (!notification.fully_loaded) return response;
  if (!content_latch_.IsNewContent(notification)) return response;
  auto str = notification.content;
  auto text = str.Render();
  NamedTempFile tmpf;
  std::vector<std::string> args;
  auto cmd =
      ClangCompileCommand(buffer_->filename(), "-", tmpf.filename(), &args);
  Log() << cmd << " " << absl::StrJoin(args, " ");
  if (run(cmd, args, text).status != 0) {
    return response;
  }

  Log() << "objdump: " << tmpf.filename();
  auto dump = run(OBJDUMP_BIN, {"-d", "-l", "-M", "intel", "-C",
                                "--no-show-raw-insn", tmpf.filename()},
                  "");

  AsmParseResult parsed_asm = AsmParse(dump.out);

  side_buffer_ref_editor_.BeginEdit(&response.side_buffer_refs);
  String::LineIterator line_it(notification.content, String::Begin());
  int line_idx = 0;
  for (const auto& m : parsed_asm.src_to_asm_line) {
    Log() << "line_idx=" << line_idx << " m.first=" << m.first;
    while (line_idx < m.first) {
      line_it.MoveNext();
      line_idx++;
    }
    side_buffer_ref_editor_.Add(
        line_it.id(),
        Annotation<SideBufferRef>(line_it.Next().id(),
                                  SideBufferRef{"disasm", m.second}));
  }
  side_buffer_ref_editor_.Publish();

  side_buffer_editor_.BeginEdit(&response.side_buffers);
  SideBuffer buf;
  buf.content.insert(buf.content.end(), parsed_asm.body.begin(),
                     parsed_asm.body.end());
  buf.tokens.resize(buf.content.size());
  buf.CalcLines();
  side_buffer_editor_.Add("disasm", std::move(buf));
  side_buffer_editor_.Publish();

  return response;
}
