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

  template <class F>
  void ForEachValue(F&& f) {
    avl_.ForEach([f](ID, const T& value) { f(value); });
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
