#pragma once

#include <assert.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "avl.h"
#include "crdt.h"

class String : public CRDT<String> {
 public:
  String() {
    avl_ =
        avl_.Add(Begin(), CharInfo{false, char(), End(), End(), End(), End()})
            .Add(End(),
                 CharInfo{false, char(), Begin(), Begin(), Begin(), Begin()});
  }

  static ID Begin() { return begin_id_; }
  static ID End() { return end_id_; }

  bool Has(ID id) const { return avl_.Lookup(id) != nullptr; }

  static ID MakeRawInsert(CommandBuf* buf, Site* site, char c, ID after,
                          ID before) {
    return MakeCommand(buf, site->GenerateID(),
                       [c, after, before](String s, ID id) {
                         return s.IntegrateInsert(id, c, after, before);
                       });
  }
  ID MakeInsert(CommandBuf* buf, Site* site, char c, ID after) const {
    return MakeRawInsert(buf, site, c, after, avl_.Lookup(after)->next);
  }

  ID MakeRemove(CommandBuf* buf, ID chr) const {
    return MakeCommand(buf, chr,
                       [](String s, ID id) { return s.IntegrateRemove(id); });
  }

  std::basic_string<char> Render() const {
    std::basic_string<char> out;
    ID cur = avl_.Lookup(Begin())->next;
    while (cur != End()) {
      const CharInfo* c = avl_.Lookup(cur);
      if (c->visible) out += c->chr;
      cur = c->next;
    }
    return out;
  }

 private:
  struct CharInfo {
    // tombstone if false
    bool visible;
    // glyph
    char chr;
    // next/prev in document
    ID next;
    ID prev;
    // before/after in insert order (according to creator)
    ID after;
    ID before;
  };

  String(AVL<ID, CharInfo> avl) : avl_(avl) {}

  String IntegrateRemove(ID id) const;
  String IntegrateInsert(ID id, char c, ID after, ID before) const;

  AVL<ID, CharInfo> avl_;
  static Site root_site_;
  static ID begin_id_;
  static ID end_id_;

 public:
  class Iterator {
   public:
    Iterator(const String& str, ID where)
        : str_(&str), pos_(where), cur_(str_->avl_.Lookup(pos_)) {
      while (!is_begin() && !is_visible()) {
        MoveBack();
      }
    }

    bool is_end() const { return pos_ == End(); }
    bool is_begin() const { return pos_ == Begin(); }

    void MoveNext() {
      if (!is_end()) MoveForward();
      while (!is_end() && !is_visible()) MoveForward();
    }

    void MovePrev() {
      if (!is_begin()) MoveBack();
      while (!is_begin() && !is_visible()) MoveBack();
    }

    Iterator Prev() {
      Iterator i(*this);
      i.MovePrev();
      return i;
    }

    ID id() const { return pos_; }
    char value() const { return cur_->chr; }

   private:
    void MoveForward() {
      pos_ = cur_->next;
      cur_ = str_->avl_.Lookup(pos_);
    }
    void MoveBack() {
      pos_ = cur_->prev;
      cur_ = str_->avl_.Lookup(pos_);
    }

    bool is_visible() const { return cur_->visible; }

    const String* str_;
    ID pos_;
    const CharInfo* cur_;
  };
};
