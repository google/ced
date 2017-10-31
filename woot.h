#pragma once

#include <assert.h>
#include <stdint.h>
#include <atomic>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "avl.h"

class Site;

typedef std::tuple<uint64_t, uint64_t> ID;

class Site {
 public:
  Site() : id_(id_gen_.fetch_add(1, std::memory_order_relaxed)) {}

  Site(const Site&) = delete;
  Site& operator=(const Site&) = delete;

  ID GenerateID() {
    return ID(id_, clock_.fetch_add(1, std::memory_order_relaxed));
  }

  uint64_t site_id() const { return id_; }

 private:
  Site(uint64_t id) : id_(id) {}
  const uint64_t id_;
  std::atomic<uint64_t> clock_{0};
  static std::atomic<uint64_t> id_gen_;
};

class StringBase {
 public:
  static ID Begin() { return begin_id_; }
  static ID End() { return end_id_; }

 private:
  static Site root_site_;
  static ID begin_id_;
  static ID end_id_;
};

template <class Char>
class String : public StringBase {
 public:
  String() {
    avl_ =
        avl_.Add(Begin(), CharInfo{false, Char(), End(), End(), End(), End()})
            .Add(End(),
                 CharInfo{false, Char(), Begin(), Begin(), Begin(), Begin()});
  }

  class Command {
   public:
    Command(ID id) : id_(id) {}
    virtual ~Command() {}

    ID id() const { return id_; }

   private:
    friend class String;
    virtual String Integrate(String string) = 0;
    const ID id_;
  };
  typedef std::unique_ptr<Command> CommandPtr;

  struct InsertResult {
    String str;
    CommandPtr command;
  };
  static CommandPtr MakeRawInsert(Site* site, Char c, ID after, ID before) {
    ID new_id = site->GenerateID();
    return CommandPtr(new InsertCommand(new_id, c, after, before));
  }
  CommandPtr MakeInsert(Site* site, Char c, ID after) {
    return MakeRawInsert(site, c, after, avl_.Lookup(after)->next);
  }
  InsertResult Insert(Site* site, Char c, ID after) {
    auto command = MakeInsert(site, c, after);
    String str = Integrate(command);
    return InsertResult{str, std::move(command)};
  }

  struct RemoveResult {
    String str;
    CommandPtr command;
  };
  RemoveResult Remove(ID chr) {
    assert(avl_.Lookup(chr));
    CommandPtr command(new DeleteCommand(chr));
    String str = Integrate(command);
    return RemoveResult{str, std::move(command)};
  }

  String Integrate(const CommandPtr& command) {
    return command->Integrate(*this);
  }

  std::basic_string<Char> Render() {
    std::basic_string<Char> out;
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
    bool visible;
    Char chr;
    ID next;
    ID prev;
    ID after;
    ID before;
  };

  String(AVL<ID, CharInfo> avl) : avl_(avl) {}

  String IntegrateDelete(ID id) {
    const CharInfo* cdel = avl_.Lookup(id);
    if (!cdel->visible) return *this;
    return String(
        avl_.Add(id, CharInfo{false, cdel->chr, cdel->next, cdel->prev,
                              cdel->after, cdel->before}));
  }

  String IntegrateInsert(ID id, Char c, ID after, ID before) {
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

  class InsertCommand final : public Command {
   public:
    InsertCommand(ID id, Char c, ID after, ID before)
        : Command(id), c_(c), after_(after), before_(before) {}

    String Integrate(String s) {
      return s.IntegrateInsert(this->id(), c_, after_, before_);
    }

   private:
    const Char c_;
    const ID after_;
    const ID before_;
  };

  class DeleteCommand final : public Command {
   public:
    DeleteCommand(ID id) : Command(id) {}

    String Integrate(String s) { return s.IntegrateDelete(this->id()); }
  };

  AVL<ID, CharInfo> avl_;

 public:
  class Iterator {
   public:
    Iterator(const String& str, ID where)
        : str_(str), pos_(where), cur_(str_.avl_.Lookup(pos_)) {}
    Iterator(const String& str)
        : str_(str),
          pos_(str_.avl_.Lookup(Begin())->next),
          cur_(str_.avl_.Lookup(pos_)) {}

    bool is_end() const { return pos_ == End(); }
    bool is_begin() const { return pos_ == Begin(); }
    bool is_visible() const { return cur_->visible; }

    void MoveNext() {
      pos_ = cur_->next;
      cur_ = str_.avl_.Lookup(pos_);
    }
    void MovePrev() {
      pos_ = cur_->prev;
      cur_ = str_.avl_.Lookup(pos_);
    }

    ID id() const { return pos_; }
    const Char* value() const { return &cur_->chr; }

   private:
    String str_;
    ID pos_;
    const CharInfo* cur_;
  };
};
