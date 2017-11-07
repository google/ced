#include <curses.h>
#include <signal.h>
#include "buffer.h"
#include "clang_format_collaborator.h"
#include "colors.h"
#include "godbolt_collaborator.h"
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
    keypad(stdscr, true);
    buffer_.MakeCollaborator<ClangFormatCollaborator>();
    buffer_.MakeCollaborator<LibClangCollaborator>();
    buffer_.MakeCollaborator<GodboltCollaborator>();
  }

  ~Application() { endwin(); }

  void Quit() {
    absl::MutexLock lock(&mu_);
    done_ = true;
  }

  void Invalidate() {
    {
      absl::MutexLock lock(&mu_);
      invalidated_ = true;
    }
    kill(getpid(), SIGWINCH);
  }

  void Run() {
    bool refresh = true;
    absl::Time last_key_press = absl::Now();
    for (;;) {
      if (refresh) {
        erase();
        Render(last_key_press);
      }
      int c = getch();
      last_key_press = absl::Now();

      refresh = true;
      switch (c) {
        case ERR:
          break;
        case KEY_RESIZE:
          refresh = false;
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

 private:
  void Render(absl::Time last_key_press) {
    terminal_collaborator_->Render(last_key_press);
  }

  absl::Mutex mu_;
  bool done_ GUARDED_BY(mu_);
  bool invalidated_ GUARDED_BY(mu_);

  Buffer buffer_;
  TerminalCollaborator *const terminal_collaborator_;
};

int main(void) { Application().Run(); }
