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
#include "woot.h"

std::atomic<uint64_t> Site::id_gen_;
Site String::root_site_;
ID String::begin_id_ = root_site_.GenerateID();
ID String::end_id_ = root_site_.GenerateID();

String String::IntegrateRemove(ID id) const {
  const CharInfo* cdel = avl_.Lookup(id);
  if (!cdel->visible) return *this;
  auto line_breaks2 = line_breaks_;
  if (cdel->chr == '\n') {
    auto* self = line_breaks2.Lookup(id);
    auto* prev = line_breaks2.Lookup(self->prev);
    auto* next = line_breaks2.Lookup(self->next);
    line_breaks2 = line_breaks2.Remove(id)
                       .Add(self->prev, LineBreak{prev->prev, self->next})
                       .Add(self->next, LineBreak{self->prev, next->next});
  }
  auto avl2 = avl_.Add(id, CharInfo{false, cdel->chr, cdel->next, cdel->prev,
                                    cdel->after, cdel->before});
  return String(avl2, line_breaks2);
}

String String::IntegrateInsert(ID id, char c, ID after, ID before) const {
  const CharInfo* caft = avl_.Lookup(after);
  const CharInfo* cbef = avl_.Lookup(before);
  assert(caft != nullptr);
  assert(cbef != nullptr);
  if (caft->next == before) {
    auto line_breaks2 = line_breaks_;
    if (c == '\n') {
      auto prev_line_id = after;
      const CharInfo* plic = caft;
      while (prev_line_id != Begin() && (!plic->visible || plic->chr != '\n')) {
        prev_line_id = plic->prev;
        plic = avl_.Lookup(prev_line_id);
      }
      auto prev_lb = line_breaks2.Lookup(prev_line_id);
      auto next_lb = line_breaks2.Lookup(prev_lb->next);
      line_breaks2 =
          line_breaks2.Add(prev_line_id, LineBreak{prev_lb->prev, id})
              .Add(id, LineBreak{prev_line_id, prev_lb->next})
              .Add(prev_lb->next, LineBreak{id, next_lb->next});
    }
    auto avl2 = avl_.Add(after, CharInfo{caft->visible, caft->chr, id,
                                         caft->prev, caft->after, caft->before})
                    .Add(id, CharInfo{true, c, before, after, after, before})
                    .Add(before, CharInfo{cbef->visible, cbef->chr, cbef->next,
                                          id, cbef->after, cbef->before});
    return String(avl2, line_breaks2);
  }
  typedef std::map<ID, const CharInfo*> LMap;
  LMap inL;
  std::vector<typename LMap::iterator> L;
  auto addToL = [&](ID id, const CharInfo* ci) {
    L.push_back(inL.emplace(id, ci).first);
  };
  addToL(after, caft);
  ID n = caft->next;
  do {
    const CharInfo* cn = avl_.Lookup(n);
    assert(cn != nullptr);
    addToL(n, cn);
    n = cn->next;
  } while (n != before);
  addToL(before, cbef);
  size_t i, j;
  for (i = 1, j = 1; i < L.size() - 1; i++) {
    auto it = L[i];
    auto ai = inL.find(it->second->after);
    if (ai == inL.end()) continue;
    auto bi = inL.find(it->second->before);
    if (bi == inL.end()) continue;
    L[j++] = L[i];
  }
  L[j++] = L[i];
  L.resize(j);
  for (i = 1; i < L.size() - 1 && L[i]->first < id; i++)
    ;
  return IntegrateInsert(id, c, L[i - 1]->first, L[i]->first);
}
