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

#include <map>
#include <set>
#include "avl.h"
#include "crdt.h"
#include "log.h"

template <class K, class V>
class UMap : public CRDT<UMap<K, V>> {
 public:
  UMap() = default;

  using CommandBuf = typename CRDT<UMap<K, V>>::CommandBuf;

  static ID MakeInsert(CommandBuf* buf, Site* site, const K& k, const V& v) {
    return MakeCommand(buf, site->GenerateID(), [k, v](UMap m, ID id) {
      auto* id2v = m.k2id2v_.Lookup(k);
      if (id2v == nullptr) {
        // first use of this key
        return UMap(m.k2id2v_.Add(k, AVL<ID, V>().Add(id, v)),
                    m.id2kv_.Add(id, std::make_pair(k, v)));
      } else {
        return UMap(m.k2id2v_.Add(k, id2v->Add(id, v)),
                    m.id2kv_.Add(id, std::make_pair(k, v)));
      }
    });
  }

  static void MakeRemove(CommandBuf* buf, ID id) {
    MakeCommand(buf, id, [](UMap m, ID id) {
      auto* p = m.id2kv_.Lookup(id);
      auto* id2v = m.k2id2v_.Lookup(p->first);
      if (id2v == nullptr) return m; // must already be removed
      auto id2v_new = id2v->Remove(id);
      if (id2v_new.Empty()) {
        return UMap(m.k2id2v_.Remove(p->first), m.id2kv_.Remove(id));
      } else {
        return UMap(m.k2id2v_.Add(p->first, id2v_new), m.id2kv_.Remove(id));
      }
    });
  }

  template <class F>
  void ForEachValue(const K& key, F&& f) const {
    auto* id2v = k2id2v_.Lookup(key);
    if (!id2v) return;
    id2v->ForEach([f](ID, const V& v) { f(v); });
  }

  template <class F>
  void ForEach(F&& f) const {
    id2kv_.ForEach(
        [&](ID id, const std::pair<K, V>& kv) { f(id, kv.first, kv.second); });
  }

 private:
  UMap(AVL<K, AVL<ID, V>>&& k2id2v, const AVL<ID, std::pair<K, V>>&& id2kv)
      : k2id2v_(std::move(k2id2v)), id2kv_(std::move(id2kv)) {}

  using CRDT<UMap<K, V>>::MakeCommand;

  AVL<K, AVL<ID, V>> k2id2v_;
  AVL<ID, std::pair<K, V>> id2kv_;
};

template <class K, class V>
class UMapEditor {
 public:
  UMapEditor(Site* site) : site_(site) {}
  void BeginEdit(typename UMap<K, V>::CommandBuf* buf) { buf_ = buf; }
  ID Add(const K& k, const V& v) {
    auto kv = std::make_pair(k, v);
    auto it = last_.find(kv);
    if (it == last_.end()) {
      return new_[kv] = UMap<K, V>::MakeInsert(buf_, site_, k, v);
    } else {
      new_.insert(*it);
      return it->second;
    }
  }
  void Publish() {
    for (auto it = last_.begin(); it != last_.end();) {
      auto next = it;
      ++next;
      if (new_.find(it->first) == new_.end()) {
        UMap<K, V>::MakeRemove(buf_, it->second);
      }
      it = next;
    }
    last_.swap(new_);
    new_.clear();
    buf_ = nullptr;
  }

 private:
  Site* const site_;
  typename UMap<K, V>::CommandBuf* buf_ = nullptr;
  std::map<std::pair<K, V>, ID> last_;
  std::map<std::pair<K, V>, ID> new_;
};
