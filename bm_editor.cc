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
#include <benchmark/benchmark.h>
#include "editor.h"

struct MockColor {
  int Theme(const Tag& tag, uint32_t flags) {
    theme_calls[std::make_pair(tag, flags)]++;
    return 0;
  }

  std::map<std::pair<Tag, uint32_t>, int> theme_calls;
};

struct MockContext {
  const Renderer<MockContext>::Rect* window;
  MockColor* color;
  bool animating;
  int* moves;
  int* putc;
  int* puts;

  void Move(int row, int col) { (*moves)++; }
  void Put(int row, int col, char c, int attr) { (*putc)++; }
  void Put(int row, int col, const std::string& s, int attr) { (*puts)++; }
};

static void BM_EditorRender(benchmark::State& state) {
  Site site;
  Editor editor(&site);
  MockColor color;
  int moves = 0;
  int putc = 0;
  int puts = 0;

  for (auto _ : state) {
    MockContext ctx{nullptr, &color, false, &moves, &putc, &puts};
    editor.Render(&ctx);
  }
}
BENCHMARK(BM_EditorRender);

BENCHMARK_MAIN()
