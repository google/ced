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
    line_breaks_ = line_breaks_.Add(Begin(), LineBreak{End(), End()})
                       .Add(End(), LineBreak{Begin(), Begin()});
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

  struct LineBreak {
    ID prev;
    ID next;
  };

  String(AVL<ID, CharInfo> avl, AVL<ID, LineBreak> line_breaks)
      : avl_(avl), line_breaks_(line_breaks) {}

  String IntegrateRemove(ID id) const;
  String IntegrateInsert(ID id, char c, ID after, ID before) const;

  AVL<ID, CharInfo> avl_;
  AVL<ID, LineBreak> line_breaks_;
  static Site root_site_;
  static ID begin_id_;
  static ID end_id_;

 public:
  class AllIterator {
   public:
    AllIterator(const String& str, ID where)
        : str_(&str), pos_(where), cur_(str_->avl_.Lookup(pos_)) {}

    bool is_end() const { return pos_ == End(); }
    bool is_begin() const { return pos_ == Begin(); }

    ID id() const { return pos_; }
    char value() const { return cur_->chr; }
    bool is_visible() const { return cur_->visible; }

    void MoveNext() {
      pos_ = cur_->next;
      cur_ = str_->avl_.Lookup(pos_);
    }
    void MovePrev() {
      pos_ = cur_->prev;
      cur_ = str_->avl_.Lookup(pos_);
    }

   private:
    const String* str_;
    ID pos_;
    const CharInfo* cur_;
  };

  class Iterator {
   public:
    Iterator(const String& str, ID where) : it_(str, where) {
      while (!is_begin() && !it_.is_visible()) {
        it_.MovePrev();
      }
    }

    bool is_end() const { return it_.is_end(); }
    bool is_begin() const { return it_.is_begin(); }

    ID id() const { return it_.id(); }
    char value() const { return it_.value(); }

    void MoveNext() {
      if (!is_end()) it_.MoveNext();
      while (!is_end() && !it_.is_visible()) it_.MoveNext();
    }

    void MovePrev() {
      if (!is_begin()) it_.MovePrev();
      while (!is_begin() && !it_.is_visible()) it_.MovePrev();
    }

    Iterator Prev() {
      Iterator i(*this);
      i.MovePrev();
      return i;
    }

   private:
    AllIterator it_;
  };

  class LineIterator {
   public:
    LineIterator(const String& str, ID where) : str_(&str) {
      Iterator it(str, where);
      const LineBreak* lb = str.line_breaks_.Lookup(it.id());
      while (lb == nullptr) {
        it.MovePrev();
        lb = str.line_breaks_.Lookup(it.id());
      }
      id_ = it.id();
    }

    void MovePrev() {
      if (id_ == Begin()) return;
      id_ = str_->line_breaks_.Lookup(id_)->prev;
    }

    void MoveNext() {
      if (id_ == End()) return;
      id_ = str_->line_breaks_.Lookup(id_)->next;
    }

    ID id() const { return id_; }

   private:
    const String* str_;
    ID id_;
  };
};

