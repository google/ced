#include "godbolt_collaborator.h"
#include "clang_config.h"
#include "log.h"
#include "temp_file.h"
#include "asm_parser.h"
#include "run.h"

EditResponse GodboltCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  if (!notification.fully_loaded) return response;
  auto str = notification.content;
  auto text = str.Render();
  if (text == last_compiled_) return response;
  last_compiled_ = text;
  NamedTempFile tmpf;
  std::vector<std::string> args;
  auto cmd = ClangCompileCommand(buffer_->filename(), "-", tmpf.filename(), &args);
  Log() << cmd;
  auto res = run(cmd, args, text);

  Log() << "objdump: " << tmpf.filename();
  auto dump = run("objdump", {"-d", "-l", "-M", "intel", "-C", "--no-show-raw-insn", tmpf.filename()}, "");

  AsmParseResult parsed_asm = AsmParse(dump);

  side_buffer_editor_.BeginEdit(&response.side_buffers);
  SideBuffer buf;
  buf.content.insert(buf.content.end(), parsed_asm.body.begin(), parsed_asm.body.end());
  buf.tokens.resize(buf.content.size());
  buf.CalcLines();
  side_buffer_editor_.Add("disasm", std::move(buf));
  side_buffer_editor_.Publish();

  return response;
}
