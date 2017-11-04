#include <curses.h>
#include "buffer.h"
#include "clang_format_collaborator.h"
#include "colors.h"
#include "libclang_collaborator.h"
#include "terminal_collaborator.h"

class Application {
 public:
  Application()
      : done_(false),
        buffer_("test.cc"),
        terminal_collaborator_(buffer_.MakeCollaborator<TerminalCollaborator>(
            [this]() { Invalidate(); })) {
    initscr();
    start_color();
    InitColors();
    bkgd(COLOR_PAIR(ColorID::DEFAULT));
    halfdelay(1);
    keypad(stdscr, true);
    input_ = std::thread([this]() { InputLoop(); });
    buffer_.MakeCollaborator<ClangFormatCollaborator>();
    buffer_.MakeCollaborator<LibClangCollaborator>();
  }

  ~Application() {
    endwin();
    input_.join();
  }

  void Quit() {
    absl::MutexLock lock(&mu_);
    done_ = true;
  }

  void Invalidate() {
    absl::MutexLock lock(&mu_);
    invalidated_ = true;
  }

  void Wait() {
    mu_.LockWhen(absl::Condition(&done_));
    mu_.Unlock();
  }

 private:
  void InputLoop() {
    bool refresh = true;
    for (;;) {
      if (refresh) {
        erase();
        Render();
      }
      int c = getch();

      refresh = true;
      switch (c) {
        case ERR: refresh = false; break;
        case KEY_RESIZE:
          break;
        case 'q':
          Quit();
          break;
        default:
          terminal_collaborator_->ProcessKey(c);
      }

      absl::MutexLock lock(&mu_);
      if (done_) return;
      if (invalidated_) {
        refresh = true;
        invalidated_ = false;
      }
    }
  }

  void Render() { terminal_collaborator_->Render(); }

  absl::Mutex mu_;
  bool done_ GUARDED_BY(mu_);
  bool invalidated_ GUARDED_BY(mu_);

  std::thread input_;

  Buffer buffer_;
  TerminalCollaborator *const terminal_collaborator_;
};

int main(void) { Application().Wait(); }
