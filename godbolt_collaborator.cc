#include "godbolt_collaborator.h"
#include "clang_config.h"
#include "log.h"
#include "subprocess.hpp"
#include "temp_file.h"

using namespace subprocess;

EditResponse GodboltCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  if (!notification.fully_loaded) return response;
  auto str = notification.content;
  auto text = str.Render();
  NamedTempFile tmpf("o");
  auto cmd = ClangCompileCommand(buffer_->filename(), "-", tmpf.filename());
  Log() << cmd;
  auto p = Popen(cmd, input{PIPE}, output{PIPE}, error{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();
  Log() << res.first.buf.data();

  Log() << "objdump: " << tmpf.filename();
  auto r = check_output(
      {"objdump", "-d", "-l", "-M", "intel", tmpf.filename().c_str()});
  Log() << r.buf.data();

  side_buffer_editor_.BeginEdit(&response.side_buffers);
  SideBuffer buf;
  buf.content.swap(r.buf);
  buf.content.resize(r.length);
  buf.tokens.resize(buf.content.size());
  buf.CalcLines();
  side_buffer_editor_.Add("disasm", std::move(buf));
  side_buffer_editor_.Publish();

  return response;
}
