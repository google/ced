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
  if (text == last_compiled_) return response;
  last_compiled_ = text;
  NamedTempFile tmpf("o");
  auto cmd = ClangCompileCommand(buffer_->filename(), "-", tmpf.filename());
  Log() << cmd;
  auto p = Popen(cmd, input{PIPE}, output{PIPE}, error{PIPE});
  p.send(text.data(), text.length());
  auto res = p.communicate();
  if (p.poll() != 0) {
    Log() << "compilation failed (returned " << p.retcode() << ")";
    return response;
  }
  Log() << res.first.buf.data();

  Log() << "objdump: " << tmpf.filename();
  auto p2 = Popen(
      {"objdump", "-d", "-l", "-M", "intel", tmpf.filename().c_str()}, input{PIPE}, output{PIPE}, error{PIPE});
  p2.send("", 0);
  auto r = p2.communicate();
  if (p2.poll() != 0) {
    Log() << "objdump failed (returned " << p2.retcode() << ")";
    return response;
  }
  Log() << r.first.buf.data();

  side_buffer_editor_.BeginEdit(&response.side_buffers);
  SideBuffer buf;
  buf.content.swap(r.first.buf);
  buf.content.resize(r.first.length);
  buf.tokens.resize(buf.content.size());
  buf.CalcLines();
  side_buffer_editor_.Add("disasm", std::move(buf));
  side_buffer_editor_.Publish();

  return response;
}
