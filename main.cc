#include <curses.h>
#include "buffer.h"
#include "clang_format_collaborator.h"
#include "io_collaborator.h"
#include "libclang_collaborator.h"
#include "terminal_collaborator.h"

class Application {
 public:
  Application()
      : done_(false),
        invalidated_(true),
        terminal_collaborator_(buffer_.MakeCollaborator<TerminalCollaborator>(
            [this]() { Invalidate(); })) {
    initscr();
    keypad(stdscr, true);
    renderer_ = std::thread([this]() { Renderer(); });
    input_ = std::thread([this]() { InputLoop(); });
    buffer_.MakeCollaborator<IOCollaborator>("test.cc");
    buffer_.MakeCollaborator<ClangFormatCollaborator>();
    buffer_.MakeCollaborator<LibClangCollaborator>();
  }

  ~Application() {
    endwin();
    input_.join();
    renderer_.join();
  }

  void Quit() {
    absl::MutexLock lock(&mu_);
    done_ = true;
    ungetch(0);
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
  void Renderer() {
    auto ready = [this]() {
      mu_.AssertHeld();
      return invalidated_ || done_;
    };

    for (;;) {
      mu_.LockWhen(absl::Condition(&ready));
      if (done_) {
        mu_.Unlock();
        return;
      }
      invalidated_ = false;
      mu_.Unlock();

      clear();
      Render();
      refresh();
    }
  }

  void InputLoop() {
    for (;;) {
      int c = getch();

      switch (c) {
        case KEY_RESIZE:
          Invalidate();
          break;
        case 'q':
          Quit();
          break;
        default:
          terminal_collaborator_->ProcessKey(c);
      }

      absl::MutexLock lock(&mu_);
      if (done_) return;
    }
  }

  void Render() { terminal_collaborator_->Render(); }

  absl::Mutex mu_;
  bool done_ GUARDED_BY(mu_);
  bool invalidated_ GUARDED_BY(mu_);

  std::thread renderer_;
  std::thread input_;

  Buffer buffer_;
  TerminalCollaborator *const terminal_collaborator_;
};

int main(void) { Application().Wait(); }
