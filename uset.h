#pragma once

#include "crdt.h"

template <class T>
class USet : public CRDT<USet<T>> {
 public:
  USet() {}

  using typename CRDT<USet<T>>::CommandBuf;

  static ID MakeInsert(CommandBuf* buf, Site* site, const T& value) {
    return MakeCommand(buf, site->GenerateID(), [value](USet<T> uset, ID id) {
      Log() << __PRETTY_FUNCTION__ << " " << std::get<0>(id) << ":"
            << std::get<1>(id);
      return USet<T>(uset.avl_.Add(id, value));
    });
  }

  static void MakeRemove(CommandBuf* buf, ID id) {
    MakeCommand(buf, id, [](USet<T> uset, ID id) {
      Log() << __PRETTY_FUNCTION__ << " " << std::get<0>(id) << ":"
            << std::get<1>(id);
      return USet<T>(uset.avl_.Remove(id));
    });
  }

 private:
  using CRDT<USet<T>>::MakeCommand;
  USet(AVL<ID, T> avl) : avl_(avl) {}

  AVL<ID, T> avl_;
};

template <class T>
class USetEditor {
 public:
  USetEditor(Site* site) : site_(site) {}
  void BeginEdit(typename USet<T>::CommandBuf* buf) { buf_ = buf; }
  ID Add(const T& v) {
    auto it = last_.find(v);
    if (it == last_.end()) {
      return new_[v] = USet<T>::MakeInsert(buf_, site_, v);
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
        USet<T>::MakeRemove(buf_, it->second);
      }
      it = next;
    }
    last_.swap(new_);
    new_.clear();
    buf_ = nullptr;
  }

 private:
  Site* const site_;
  typename USet<T>::CommandBuf* buf_ = nullptr;
  std::map<T, ID> last_;
  std::map<T, ID> new_;
};
