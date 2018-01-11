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
#include "client_collaborator.h"
#include "render.h"
#include "terminal_color.h"

// include last: curses.h obnoxiously #define's OK
#include <curses.h>

class Curses final : public Application, public Device, public Invalidator {
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

  void Invalidate() override {
    invalidated_ = true;
    kill(getpid(), SIGWINCH);
  }

  void ClipboardPut(const std::string& s) override { clipboard_ = s; }

  std::string ClipboardGet() override { return clipboard_; }

  void Paint(Renderer* renderer, const Widget& widget) override {
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

      int width() const override { return right_ - left_; }
      int height() const override { return bottom_ - top_; }

      void Fill(float left, float top, float right, float bottom,
                Color color) override {
        left = std::max(left_, static_cast<int>(left) + ofsx_);
        right = std::min(right_, static_cast<int>(right) + ofsx_);
        if (left >= right) return;
        top = std::max(top_, static_cast<int>(top) + ofsy_);
        bottom = std::min(bottom_, static_cast<int>(bottom) + ofsy_);
        if (top >= bottom) return;
        for (int y = top; y < bottom; y++) {
          for (int x = left; x < right; x++) {
            Cell& cell = c_->cells_[y * c_->fb_cols_ + x];
            cell.c = ' ';
            cell.fmt.background = color;
          }
        }
      }

      void PutText(float x, float y, const TextElem* text,
                   size_t length) override {
        x += ofsx_;
        y += ofsy_;
        if (y < top_) return;
        if (y >= bottom_) return;
        if (x < left_) return;
        if (x >= right_) return;
        for (int i = 0; i < length && x + i < right_; i++) {
          Cell& cell = c_->cells_[y * c_->fb_cols_ + x + i];
          cell.c = text[i].ch;
          cell.fmt.foreground = text[i].color;
          cell.fmt.highlight = text[i].highlight;
        }
      }

      void PutCaret(float x, float y, unsigned flags, Color color) override {
        if (flags == CARET_PRIMARY) {
          Log() << "MoveCursor: " << x << "," << y << ": ofs=" << ofsx_ << ","
                << ofsy_;
          c_->cursor_row_ = -1;
          c_->cursor_col_ = -1;
          const int r = y + ofsy_;
          const int c = x + ofsx_;
          if (r < top_) return;
          if (r >= bottom_) return;
          if (c < left_) return;
          if (c >= right_) return;
          c_->cursor_row_ = r;
          c_->cursor_col_ = c;
        } else {
          const int r = y + ofsy_;
          const int c = x + ofsx_;
          if (r < top_) return;
          if (r >= bottom_) return;
          if (c < left_) return;
          if (c >= right_) return;
          c_->cells_[r * c_->fb_cols_ + c].fmt.background = color;
        }
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
    Log() << "WIDGET " << widget.id() << " type " << widget.type()
          << " parent (" << parent_left << "," << parent_top << ")-("
          << parent_right << "," << parent_bottom << ")"
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
    return Extents{static_cast<float>(fb_rows), static_cast<float>(fb_cols), 1,
                   1};
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
      int c = invalidated_ ? -1 : getch();
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
        case KEY_SLEFT:
          renderer_.AddKbEvent("S-left");
          break;
        case KEY_LEFT:
          renderer_.AddKbEvent("left");
          break;
        case KEY_SRIGHT:
          renderer_.AddKbEvent("S-right");
          break;
        case KEY_RIGHT:
          renderer_.AddKbEvent("right");
          break;
        case KEY_HOME:
          renderer_.AddKbEvent("home");
          break;
        case KEY_END:
          renderer_.AddKbEvent("end");
          break;
        case KEY_PPAGE:
          renderer_.AddKbEvent("page-up");
          break;
        case KEY_NPAGE:
          renderer_.AddKbEvent("page-down");
          break;
        case KEY_SR:  // shift down on my mac
          renderer_.AddKbEvent("S-up");
          break;
        case KEY_SF:
          renderer_.AddKbEvent("S-down");
          break;
        case KEY_UP:
          renderer_.AddKbEvent("up");
          break;
        case KEY_DOWN:
          renderer_.AddKbEvent("down");
          break;
        case '\n':
          renderer_.AddKbEvent("ret");
          break;
        case 127:
        case KEY_BACKSPACE:
          renderer_.AddKbEvent("del");
          break;
        default:
          if (c >= 1 && c < 32) {
            char code[4] = {'C', '-', static_cast<char>(c + 'a' - 1), 0};
            renderer_.AddKbEvent(code);
          } else if (c >= 32 && c < 127) {
            char ch = c;
            renderer_.AddKbEvent(absl::string_view(&ch, 1));
          }
      }

      log_timer->Mark("input_processed");

      bool expected = true;
      if (!invalidated_.compare_exchange_strong(expected, false,
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
    getmaxyx(stdscr, fb_rows_, fb_cols_);
    cells_.resize(fb_rows_ * fb_cols_);
    renderer_.BeginFrame();
    auto top = renderer_.MakeRow(Widget::Options().set_id("top"));
    RenderContainers containers{
        top->MakeRow(Widget::Options().set_id("main")),
        top->MakeColumn(Widget::Options().set_id("side_bar")),
    };
    ClientCollaborator::All_Render(containers, color_->theme());
    log_timer->Mark("collected_layout");
    absl::optional<absl::Time> next_frame = renderer_.FinishFrame();
    log_timer->Mark("drawn");

    for (int i = 0; i < fb_rows_; i++) {
      for (int j = 0; j < fb_cols_; j++) {
        auto cell = cells_[i * fb_cols_ + j];
        mvaddch(i, j, cell.c | color_->Lookup(cell.fmt));
      }
    }

    auto frame_time = absl::Now() - last_key_press;
    std::ostringstream out;
    out << frame_time;
    std::string ftstr = out.str();
    chtype attr = frame_time > absl::Milliseconds(10)
                      ? color_->Theme({"invalid"}, 0)
                      : color_->Theme({}, 0);
    for (size_t i = 0; i < ftstr.length(); i++) {
      mvaddch(fb_rows_ - 1, fb_cols_ - ftstr.length() + i, ftstr[i] | attr);
    }

    Log() << "finally move cursor " << cursor_row_ << ", " << cursor_col_;
    if (cursor_row_ != -1) {
      move(cursor_row_, cursor_col_);
      curs_set(1);
    } else {
      curs_set(0);
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
  std::string clipboard_;
  std::atomic<bool> invalidated_;
  struct Cell {
    char c;
    CharFmt fmt;
  };
  int fb_rows_;
  int fb_cols_;
  std::vector<Cell> cells_;
};

REGISTER_APPLICATION(Curses);
