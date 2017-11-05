#include "woot.h"

std::atomic<uint64_t> Site::id_gen_;
Site String::root_site_;
ID String::begin_id_ = root_site_.GenerateID();
ID String::end_id_ = root_site_.GenerateID();

String String::IntegrateRemove(ID id) const {
  const CharInfo* cdel = avl_.Lookup(id);
  if (!cdel->visible) return *this;
  return String(avl_.Add(id, CharInfo{false, cdel->chr, cdel->next, cdel->prev,
                                      cdel->after, cdel->before}));
}

String String::IntegrateInsert(ID id, char c, ID after, ID before) const {
  const CharInfo* caft = avl_.Lookup(after);
  const CharInfo* cbef = avl_.Lookup(before);
  assert(caft != nullptr);
  assert(cbef != nullptr);
  if (caft->next == before) {
    return String(
        avl_.Add(after, CharInfo{caft->visible, caft->chr, id, caft->prev,
                                 caft->after, caft->before})
            .Add(id, CharInfo{true, c, before, after, after, before})
            .Add(before, CharInfo{cbef->visible, cbef->chr, cbef->next, id,
                                  cbef->after, cbef->before}));
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
