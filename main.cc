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
#include <curses.h>
#include <signal.h>
#include "buffer.h"
#include "render.h"
#include "terminal_collaborator.h"
#include "terminal_color.h"

constexpr char ctrl(char c) { return c & 037; }

class Application {
 public:
  Application(const char* filename)
      : done_(false),
        buffer_(filename) {
          assert(!app_);
          app_ = this;
    auto theme = std::unique_ptr<Theme>(new Theme(Theme::DEFAULT));
    initscr();
    raw();
    noecho();
    set_escdelay(25);
    start_color();
    color_.reset(new TerminalColor{std::move(theme)});
    bkgd(color_->Theme({}, 0));
    keypad(stdscr, true);
  }

  ~Application() { assert(app_==this); endwin(); app_=nullptr;}

  static Application* Get() { return app_; }

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
    std::unique_ptr<LogTimer> log_timer(new LogTimer("main_loop"));
    for (;;) {
      bool animating = false;
      if (refresh) {
        erase();
        animating = Render(log_timer.get(), last_key_press);
        log_timer->Mark("rendered");
      }
      timeout(animating ? 10 : -1);
      log_timer.reset();
      int c = getch();
      log_timer.reset(new LogTimer("main_loop"));
      Log() << "GOTKEY: " << c;
      last_key_press = absl::Now();

      refresh = true;
      switch (c) {
        case ERR:
          break;
        case KEY_RESIZE:
          refresh = false;
          break;
        case 27:
          Quit();
          break;
        default:
          TerminalCollaborator::All_ProcessKey(&app_env_, c);
      }

      log_timer->Mark("input_processed");

      absl::MutexLock lock(&mu_);
      if (done_) return;
      if (invalidated_) {
        refresh = true;
        invalidated_ = false;
      }

      log_timer->Mark("signalled");
    }
  }

 private:
  bool Render(LogTimer* log_timer, absl::Time last_key_press) {
    TerminalRenderer renderer;
    int fb_rows, fb_cols;
    getmaxyx(stdscr, fb_rows, fb_cols);
    auto top = renderer.AddContainer(LAY_COLUMN).FixSize(fb_cols, fb_rows);
    auto body = top.AddContainer(LAY_FILL, LAY_ROW);
    TerminalRenderContainers containers{
        body.AddContainer(LAY_LEFT | LAY_VFILL, LAY_ROW),
        body.AddContainer(LAY_FILL, LAY_COLUMN),
        top.AddContainer(LAY_BOTTOM | LAY_HFILL, LAY_COLUMN),
        top.AddContainer(LAY_HFILL, LAY_ROW).FixSize(0, 1),
    };
    TerminalCollaborator::All_Render(containers);
    log_timer->Mark("collected_layout");
    renderer.Layout();
    log_timer->Mark("layout");
    TerminalRenderContext ctx{color_.get(), nullptr, -1, -1, false};
    renderer.Draw(&ctx);
    log_timer->Mark("draw");

    auto frame_time = absl::Now() - last_key_press;
    std::ostringstream out;
    out << frame_time;
    std::string ftstr = out.str();
    chtype attr = frame_time > absl::Milliseconds(10)
                      ? color_->Theme({"invalid"}, 0)
                      : color_->Theme({}, 0);
    for (size_t i = 0; i < ftstr.length(); i++) {
      mvaddch(fb_rows - 1, fb_cols - ftstr.length() + i, ftstr[i] | attr);
    }

    if (ctx.crow != -1) {
      move(ctx.crow, ctx.ccol);
    }

    return ctx.animating;
  }

  absl::Mutex mu_;
  bool done_ GUARDED_BY(mu_);
  bool invalidated_ GUARDED_BY(mu_);

  Buffer buffer_;
  std::unique_ptr<TerminalColor> color_;
  AppEnv app_env_;

  static Application* app_;
};

void InvalidateTerminal() {
  Application::Get()->Invalidate();
}

Application* Application::app_ = nullptr;

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "USAGE: ced <filename.{h,cc}>\n");
    return 1;
  }
  try {
    Application(argv[1]).Run();
  } catch (std::exception& e) {
    Log() << "FATAL ERROR: " << e.what();
    fprintf(stderr, "ERROR: %s", e.what());
    _exit(1);
  }
  return 0;
}
