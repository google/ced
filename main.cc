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
#include <gflags/gflags.h>
#include <signal.h>
#include "buffer.h"
#include "render.h"
#include "terminal_collaborator.h"
#include "terminal_color.h"
#include "src_hash.h"

constexpr char ctrl(char c) { return c & 037; }

static std::atomic<bool> invalidated;

class Application : public std::enable_shared_from_this<Application> {
 public:
  Application(const char* filename) : done_(false), buffer_(filename) {
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

  ~Application() { endwin(); }

  void Quit() { done_ = true; }

  void Run() {
    bool was_invalidated = false;
    absl::Time last_key_press = absl::Now();
    std::unique_ptr<LogTimer> log_timer(new LogTimer("main_loop"));
    for (;;) {
      bool animating = false;
      if (was_invalidated) {
        animating = true;
      } else {
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

      was_invalidated = false;
      switch (c) {
        case ERR:
          break;
        case KEY_RESIZE:
          was_invalidated = true;
          break;
        case 27:
          Quit();
          break;
        default:
          TerminalCollaborator::All_ProcessKey(&app_env_, c);
      }

      log_timer->Mark("input_processed");

      if (done_) return;
      bool expected = true;
      was_invalidated &= (invalidated.compare_exchange_strong(expected, false,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed));

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

  std::atomic<bool> done_;
  Buffer buffer_;
  std::unique_ptr<TerminalColor> color_;
  AppEnv app_env_;
};

void InvalidateTerminal() {
  invalidated = true;
  kill(getpid(), SIGWINCH);
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage("ced <filename.{h,cc}>");
  gflags::SetVersionString(ced_src_hash);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (argc != 2) {
    gflags::ShowUsageWithFlags(argv[0]);
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
