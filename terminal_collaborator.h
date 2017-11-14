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
#pragma once

#include <curses.h>
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "buffer.h"
#include "editor.h"
#include "log.h"
#include "render.h"
#include "terminal_color.h"

struct TerminalRenderContext {
  TerminalColor* const color;
  const Renderer<TerminalRenderContext>::Rect* window;

  int crow;
  int ccol;

  void Put(int row, int col, chtype ch, chtype attr) {
    if (row < 0 || col < 0 || col >= window->width() ||
        row >= window->height()) {
      return;
    }
    mvaddch(row + window->row(), col + window->column(), ch | attr);
  }

  void Put(int row, int col, const std::string& str, chtype attr) {
    for (auto c : str) {
      Put(row, col++, c, attr);
    }
  }

  void Move(int row, int col) {
    Log() << "TRMOVE: " << row << ", " << col;
    if (row < 0 || col < 0 || col >= window->width() ||
        row >= window->height()) {
      crow = ccol = -1;
    } else {
      crow = row + window->row();
      ccol = col + window->column();
    }
  }
};

typedef Renderer<TerminalRenderContext> TerminalRenderer;

class TerminalCollaborator final : public Collaborator {
 public:
  TerminalCollaborator(const Buffer* buffer, std::function<void()> invalidate);
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;

  void Render(TerminalRenderer::ContainerRef container);
  void ProcessKey(AppEnv* app_env, int key);

 private:
  const std::function<void()> invalidate_;
  absl::Mutex mu_;
  Editor editor_ GUARDED_BY(mu_);
  bool recently_used_ GUARDED_BY(mu_);
};
