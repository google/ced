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

#include <signal.h>
#include <atomic>
#include "application.h"
#include "buffer.h"
#include "client.h"
#include "render.h"
#include "terminal_collaborator.h"
#include "terminal_color.h"

// include last: curses.h obnoxiously #define's OK
#include <curses.h>

static std::atomic<bool> invalidated;
void InvalidateTerminal() {
  Log() << "InvalidatedTerminal";
  invalidated = true;
  kill(getpid(), SIGWINCH);
}

class Curses final : public Application, public Device {
 public:
  Curses(int argc, char** argv)
      : client_(argv[0], FileFromCmdLine(argc, argv)) {
    Log::SetCerrLog(false);
    auto theme = std::unique_ptr<Theme>(new Theme(Theme::DEFAULT));
    initscr();
    raw();
    noecho();
    set_escdelay(25);
    start_color();
    color_.reset(new TerminalColor{std::move(theme)});
    bkgd(color_->Theme({}, 0));
    keypad(stdscr, true);
    buffer_ = client_.MakeBuffer(FileFromCmdLine(argc, argv));
  }

  ~Curses() {
    endwin();
    Log::SetCerrLog(true);
  }

  void Paint(const Renderer* renderer, const Widget& widget) override {
    auto e = renderer->extents();  // grab cached copy
    PaintWindow(widget, 0, 0, e.win_width, e.win_height);
  }

  void PaintWindow(const Widget& widget, int parent_left, int parent_top,
                   int parent_right, int parent_bottom) {
    class Ctx final : public DeviceContext {
     public:
      Ctx(Curses* c, int ofsx, int ofsy, int left, int top, int right,
          int bottom)
          : c_(c),
            ofsx_(ofsx),
            ofsy_(ofsy),
            left_(left),
            top_(top),
            right_(right),
            bottom_(bottom) {}

      int width() const { return right_ - left_; }
      int height() const { return bottom_ - top_; }

      void PutChar(int row, int col, uint32_t chr, CharFmt fmt) {
        assert(chr >= 32 && chr < 127);
        const int r = row + ofsy_;
        const int c = col + ofsx_;
        if (r < top_) return;
        if (r >= bottom_) return;
        if (c < left_) return;
        if (c >= right_) return;
        mvaddch(r, c, chr | c_->color_->Lookup(fmt));
      }

      void MoveCursor(int row, int col) {
        c_->cursor_row_ = row + ofsy_;
        c_->cursor_col_ = col + ofsx_;
      }

     private:
      Curses* const c_;
      const int ofsx_;
      const int ofsy_;
      const int left_;
      const int top_;
      const int right_;
      const int bottom_;
    };
    int left = std::max(static_cast<int>(widget.left().value()) + parent_left,
                        parent_left);
    int top = std::max(static_cast<int>(widget.top().value()) + parent_top,
                       parent_top);
    int right = std::min(static_cast<int>(widget.right().value()) + parent_left,
                         parent_right);
    int bottom = std::min(
        static_cast<int>(widget.bottom().value()) + parent_top, parent_bottom);
    Log() << "WIDGET " << widget.id() << " parent (" << parent_left << ","
          << parent_top << ")-(" << parent_right << "," << parent_bottom << ")"
          << " self (" << left << "," << top << ")-(" << right << "," << bottom
          << ")";
    Ctx ctx(this, parent_left, parent_top, left, top, right, bottom);
    widget.PaintSelf(&ctx);
    for (const auto* c : widget.children()) {
      PaintWindow(*c, left, top, right, bottom);
    }
  }

  Extents GetExtents() override {
    int fb_rows, fb_cols;
    getmaxyx(stdscr, fb_rows, fb_cols);
    return Extents{fb_rows, fb_cols, 1, 1};
  }

  int Run() override {
    bool was_invalidated = false;
    absl::Time last_key_press = absl::Now();
    std::unique_ptr<LogTimer> log_timer(new LogTimer("main_loop"));
    for (;;) {
      absl::optional<absl::Time> next_frame_time;
      if (was_invalidated) {
        next_frame_time = absl::Now() + absl::Milliseconds(16);
        log_timer->Mark("animating_due_to_invalidated");
      } else {
        erase();
        next_frame_time = Render(log_timer.get(), last_key_press);
        log_timer->Mark("rendered");
      }
      absl::Time now = absl::Now();
      timeout(next_frame_time && *next_frame_time >= now
                  ? ToInt64Milliseconds(*next_frame_time - now)
                  : -1);
      if (!was_invalidated) log_timer.reset();
      int c = invalidated ? -1 : getch();
      if (!was_invalidated) log_timer.reset(new LogTimer("main_loop"));
      Log() << "GOTKEY: " << c;
      if (!was_invalidated) last_key_press = absl::Now();

      log_timer->Mark("XXX");

      was_invalidated = false;
      switch (c) {
        case ERR:
        case KEY_RESIZE:
          was_invalidated = true;
          break;
        case 27:
          return 0;
        default:
          TerminalCollaborator::All_ProcessKey(&app_env_, c);
      }

      log_timer->Mark("input_processed");

      bool expected = true;
      if (!invalidated.compare_exchange_strong(expected, false,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
        log_timer->Mark("clear_invalidated");
        was_invalidated = false;
      }

      if (was_invalidated) {
        log_timer->Mark("signalled");
      } else {
        log_timer->Mark("next_loop");
      }
    }
  }

 private:
  absl::optional<absl::Time> Render(LogTimer* log_timer,
                                    absl::Time last_key_press) {
    int fb_rows, fb_cols;
    getmaxyx(stdscr, fb_rows, fb_cols);
    renderer_.BeginFrame();
    auto top = renderer_.MakeRow();
    TerminalRenderContainers containers{
        top->MakeRow(), top->MakeColumn(),
    };
    TerminalCollaborator::All_Render(containers, color_->theme());
    log_timer->Mark("collected_layout");
    absl::optional<absl::Time> next_frame = renderer_.FinishFrame();
    log_timer->Mark("drawn");
    Log() << "top: (" << top->left().value() << "," << top->top().value()
          << ")-(" << top->right().value() << "," << top->bottom().value()
          << ")   "
          << "main: (" << containers.main->left().value() << ","
          << containers.main->top().value() << ")-("
          << containers.main->right().value() << ","
          << containers.main->bottom().value() << ")   "
          << "side_bar: (" << containers.side_bar->left().value() << ","
          << containers.side_bar->top().value() << ")-("
          << containers.side_bar->right().value() << ","
          << containers.side_bar->bottom().value() << ")";

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

    if (cursor_row_ != -1) {
      move(cursor_row_, cursor_col_);
    }

    return next_frame;
  }

  static boost::filesystem::path FileFromCmdLine(int argc, char** argv) {
    if (argc != 2) {
      throw std::runtime_error("Expected filename to open");
    }
    return argv[1];
  }

  Client client_;
  Renderer renderer_{this};
  int cursor_row_ = -1;
  int cursor_col_ = -1;
  std::unique_ptr<Buffer> buffer_;
  std::unique_ptr<TerminalColor> color_;
  AppEnv app_env_;
};

REGISTER_APPLICATION(Curses);
