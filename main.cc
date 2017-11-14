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
#include "clang_format_collaborator.h"
#include "godbolt_collaborator.h"
#include "libclang_collaborator.h"
#include "render.h"
#include "terminal_collaborator.h"
#include "terminal_color.h"

constexpr char ctrl(char c) { return c & 037; }

class Application {
 public:
  Application(const char* filename)
      : done_(false),
        buffer_(filename),
        terminal_collaborator_(buffer_.MakeCollaborator<TerminalCollaborator>(
            [this]() { Invalidate(); })) {
    auto theme = std::unique_ptr<Theme>(new Theme(Theme::DEFAULT));
    initscr();
    raw();
    set_escdelay(25);
    start_color();
    color_.reset(new TerminalColor{std::move(theme)});
    bkgd(color_->Theme(Tag(), 0));
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
          terminal_collaborator_->ProcessKey(&app_env_, c);
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
    TerminalRenderer renderer;
    int fb_rows, fb_cols;
    getmaxyx(stdscr, fb_rows, fb_cols);
    auto top = renderer.AddContainer(LAY_COLUMN).FixSize(fb_cols, fb_rows);
    TerminalRenderContainers containers{
        top.AddContainer(LAY_FILL, LAY_ROW),
        top.AddContainer(LAY_BOTTOM | LAY_HFILL, LAY_COLUMN),
        top.AddContainer(LAY_HFILL, LAY_ROW).FixSize(0, 1),
    };
    terminal_collaborator_->Render(containers);
    renderer.Layout();
    TerminalRenderContext ctx{color_.get(), nullptr, -1, -1};
    renderer.Draw(&ctx);

    Log() << "cursor:: " << ctx.crow << ", " << ctx.ccol;

    auto frame_time = absl::Now() - last_key_press;
    std::ostringstream out;
    out << frame_time;
    std::string ftstr = out.str();
    mvaddstr(fb_rows - 1, fb_cols - ftstr.length(), ftstr.c_str());

    if (ctx.crow != -1) {
      move(ctx.crow, ctx.ccol);
    }
  }

  absl::Mutex mu_;
  bool done_ GUARDED_BY(mu_);
  bool invalidated_ GUARDED_BY(mu_);

  Buffer buffer_;
  TerminalCollaborator* const terminal_collaborator_;
  std::unique_ptr<TerminalColor> color_;
  AppEnv app_env_;
};

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "USAGE: ced <filename.{h,cc}>\n");
    return 1;
  }
  try {
    Application(argv[1]).Run();
  } catch (std::exception& e) {
    fprintf(stderr, "ERROR: %s", e.what());
    _exit(1);
  }
  return 0;
}
