#pragma once

#include "crdt.h"

template <class K, class V>
class UMap : public CRDT<UMap<K, V>> {
 public:
UMap() = default;

  static ID MakeInsert(typename UMap::CommandBuf* buf, Site* site, const K& k, const V& v) {
    return MakeCommand(buf, site->GenerateID(), [k, v](UMap m, ID id) {
      auto* id2v = m.k2id2v_.Lookup(k);
      if (id2v == nullptr) {
        // first use of this key
        return UMap(m.k2id2v_.Add(k, AVL<ID, V>().Add(id, v),
                                m.id2kv_.Add(id, std::make_pair(k, v))));
      } else {
        return UMap(m.k2id2v_.Add(k, id2v->Add(id, v),
                                m.id2kv_.Add(id, std::make_pair(k, v))));
      }
    });
  }

  static void MakeRemove(typename UMap::CommandBuf* buf, ID id) {
    return MakeCommand(buf, id, [](UMap m, ID id) {
      auto* p = m.id2kv_.Lookup(id);
      auto* id2v = m.k2id2v_.Lookup(p->first);
      auto id2v_new = id2v->Remove(id);
      if (id2v_new.Empty()) {
        return UMap(m.k2id2v_.Remove(p->first), m.id2kv_.Remove(id));
      } else {
        return UMap(m.k2id2v_.Add(p->first, id2v_new), m.id2kv_.Remove(id));
      }
    });
  }

  template <class F>
  void ForEachValue(const K& key, F&& f) {
    auto* id2v = k2id2v_.Lookup(key);
    if (!id2v) return;
    id2v->ForEach([f](ID, const V& v) { f(v); });
  }

 private:
UMap(AVL<K, AVL<ID,V>>& k2id2v, const AVL<ID, std::pair<K,V>>&& id2kv) : k2id2v_(std::move(k2id2v)), id2kv_(std::move(id2kv_)) {}

  AVL<K, AVL<ID, V>> k2id2v_;
  AVL<ID, std::pair<K, V>> id2kv_;
};
