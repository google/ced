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

static std::string GenLines(const std::string& base, int n) {
  std::string out;
  for (int i = 0; i < n; i++) {
    out.append(base);
    out += '\n';
  }
  return out;
}

static void BM_EditorRender(benchmark::State& state) {
  Site site;
  Editor editor(&site);
  MockColor color;
  int moves = 0;
  int putc = 0;
  int puts = 0;
  Renderer<MockContext>::Rect win(lay_vec4{0, 0, 200, 100});

  EditNotification n;
  EditResponse r;
  String::MakeRawInsert(&r.content, &site,
                        "int foo(int i) {\n" +
                            GenLines("i += 123456789;", state.range(0)) + "}\n",
                        String::Begin(), String::End());
  IntegrateResponse(r, &n);
  editor.UpdateState(n);

  for (auto _ : state) {
    MockContext ctx{&win, &color, false, &moves, &putc, &puts};
    editor.Render(&ctx);
  }

  state.counters["moves"] = moves;
  state.counters["putc"] = putc;
  state.counters["puts"] = puts;
}
BENCHMARK(BM_EditorRender)->Range(1, 32768);

static void BM_EditorRenderSideBar(benchmark::State& state) {
  Site site;
  Editor editor(&site);
  MockColor color;
  int moves = 0;
  int putc = 0;
  int puts = 0;
  Renderer<MockContext>::Rect win(lay_vec4{0, 0, 200, 100});

  EditResponse r;
  EditNotification n;
  SideBuffer sb;
  for (auto c : GenLines("i += 123456789;", state.range(0))) {
    sb.content.push_back(c);
    sb.tokens.push_back(Tag());
  }
  sb.CalcLines();
  UMap<std::string, SideBuffer>::MakeInsert(&r.side_buffers, &site, "disasm",
                                            sb);
  AnnotationMap<SideBufferRef>::MakeInsert(
      &r.side_buffer_refs, &site, String::Begin(),
      Annotation<SideBufferRef>(String::End(),
                                SideBufferRef{"disasm", {1, 2, 3, 4, 5, 6}}));
  IntegrateResponse(r, &n);
  editor.UpdateState(n);

  {
    MockContext ctx{&win, &color, false, &moves, &putc, &puts};
    editor.Render(&ctx);
  }

  for (auto _ : state) {
    MockContext ctx{&win, &color, false, &moves, &putc, &puts};
    editor.RenderSideBar(&ctx);
  }

  state.counters["moves"] = moves;
  state.counters["putc"] = putc;
  state.counters["puts"] = puts;
}
BENCHMARK(BM_EditorRenderSideBar)->Range(1, 32768);

BENCHMARK_MAIN()
