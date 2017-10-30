#include "buffer.h"
#include "fullscreen.h"
#include "io_collaborator.h"
#include "terminal_collaborator.h"

int main(void) {
  tl::Fullscreen terminal;
  ced::buffer::Buffer buffer;
  auto* terminal_collaborator =
      buffer.MakeCollaborator<ced::buffer::TerminalCollaborator>(&terminal);
  buffer.MakeCollaborator<ced::buffer::IOCollaborator>("avl.h");

  absl::Mutex mu;
  bool done = false;

  auto set_done = [&]() {
    mu.Lock();
    done = true;
    mu.Unlock();
  };

  std::function<void(absl::optional<tl::Key>)> inp_cb =
      [&](absl::optional<tl::Key> c) {
        if (!c) {
          set_done();
          return;
        }
        if (c->is_character() && tolower(c->character()) == 'q' &&
            c->mods() == tl::Key::CTRL) {
          set_done();
          return;
        }
        terminal_collaborator->ProcessKey(*c);
        terminal.NextInput(inp_cb);
      };
  std::function<void(tl::FrameBuffer*)> rdr_cb = [&](tl::FrameBuffer* fb) {
    if (!fb) {
      set_done();
      return;
    }
    terminal_collaborator->Render(fb);
    terminal.NextRender(rdr_cb);
  };

  terminal.NextInput(inp_cb);
  terminal.NextRender(rdr_cb);

  mu.LockWhen(absl::Condition(&done));
  mu.Unlock();
}
