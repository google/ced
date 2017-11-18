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

std::atomic<uint16_t> Site::id_gen_{1};
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
  if (avl_.Lookup(id)) return *this;
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

int String::OrderIDs(ID a, ID b) const {
  // same id
  if (a == b) return 0;
  if (a == Begin()) return -1;
  if (a == End()) return 1;
  if (b == Begin()) return 1;
  if (b == End()) return -1;
  LineIterator line_a(*this, a);
  LineIterator line_b(*this, b);
  if (line_a.id() == line_b.id()) {
    // same line
    AllIterator it(*this, line_a.id());
    for (;;) {
      if (it.id() == a) return -1;
      if (it.id() == b) return 1;
      it.MoveNext();
    }
  }
  LineIterator bk = line_a;
  LineIterator fw = line_a;
  for (;;) {
    bk.MovePrev();
    fw.MoveNext();
    if (bk.id() == line_b.id()) return 1;
    if (fw.id() == line_b.id()) return -1;
    if (bk.is_begin()) return -1;
    if (fw.is_end()) return 1;
  }
}

std::string String::Render() const { return Render(Begin(), End()); }

std::string String::Render(ID beg, ID end) const {
  if (OrderIDs(beg, end) > 0) {
    std::swap(beg, end);
  }

  std::string out;
  ID cur = beg;
  while (cur != end) {
    const CharInfo* c = avl_.Lookup(cur);
    if (c->visible) out += c->chr;
    cur = c->next;
  }
  return out;
}

void String::MakeRemove(CommandBuf* buf, ID beg, ID end) const {
  if (OrderIDs(beg, end) > 0) {
    std::swap(beg, end);
  }

  AllIterator it(*this, beg);
  while (it.id() != end) {
    if (it.is_visible()) MakeRemove(buf, it.id());
    it.MoveNext();
  }
}
