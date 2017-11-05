#pragma once

#include "crdt.h"

template <class T>
class USet : public CRDT<USet<T>> {
 public:
  USet() {}

  static ID MakeInsert(CommandBuf* buf, Site* site, const T& value) {
    return MakeCommand(buf, site->GenerateID(), [value](USet<T> uset, ID id) {
      return USet<T>(avl_.Add(id, value));
    });
  }

  static void MakeRemove(CommandBuf* buf, ID id) {
    MakeCommand(buf, id, [](USet<T> uset, ID id) {
      return USet<T>(avl_.Remove(id));
    });
  }

 private:
  USet(AVL<ID, T> avl) : avl_(avl) {}

  AVL<ID, T> avl_;
};

