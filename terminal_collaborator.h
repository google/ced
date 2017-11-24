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
#include "line_editor.h"
#include "log.h"
#include "render.h"
#include "terminal_color.h"

struct TerminalRenderContext {
  TerminalColor* const color;
  const Renderer<TerminalRenderContext>::Rect* window;

  int crow;
  int ccol;
  bool animating;

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

struct TerminalRenderContainers {
  TerminalRenderer::ContainerRef main;
  TerminalRenderer::ContainerRef side_bar;
  TerminalRenderer::ContainerRef ext_status;
  TerminalRenderer::ContainerRef status;
};

extern void InvalidateTerminal();

class TerminalCollaborator final : public AsyncCollaborator {
 public:
  TerminalCollaborator(const Buffer* buffer);
  ~TerminalCollaborator();
  void Push(const EditNotification& notification) override;
  EditResponse Pull() override;

  static void All_Render(TerminalRenderContainers containers);
  static void All_ProcessKey(AppEnv* app_env, int key);

 private:
  enum class State {
    EDITING,
    FINDING,
  };

  void Render(TerminalRenderContainers containers)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void ProcessKey(AppEnv* app_env, int key) EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const Buffer* const buffer_;
  static absl::Mutex mu_;
  Editor editor_ GUARDED_BY(mu_);
  LineEditor find_editor_ GUARDED_BY(mu_);
  bool recently_used_ GUARDED_BY(mu_);
  State state_ GUARDED_BY(mu_);

  static std::vector<TerminalCollaborator*> all_ GUARDED_BY(mu_);
};
