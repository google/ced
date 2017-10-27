#pragma once

#include <assert.h>
#include <stdint.h>
#include <memory>
#include <tuple>
#include <vector>

#include "avl.h"

namespace woot {

class Site;
typedef std::shared_ptr<Site> SitePtr;

typedef std::tuple<SitePtr, uint64_t> ID;

class Site : public std::enable_shared_from_this<Site> {
 public:
  ID GenerateID() {
    return ID(shared_from_this(),
              clock_.fetch_add(1, std::memory_order_relaxed));
  }

  static SitePtr Make() { return SitePtr(new Site); }

 private:
  Site() {}
  std::atomic<uint64_t> clock_{0};
};

template <class Char>
class String {
 public:
  String() {
    begin_ = root_site()->GenerateID();
    end_ = root_site()->GenerateID();
    avl_ =
        avl_.Add(begin_, CharInfo{false, Char(), end_, end_, end_, end_})
            .Add(end_, CharInfo{false, Char(), begin_, begin_, begin_, begin_});
  }

  class Command {
   public:
    virtual ~Command() {}

   private:
    friend class String;
    virtual String Integrate(String string) = 0;
  };
  typedef std::unique_ptr<Command> CommandPtr;

  ID begin() { return begin_; }
  ID end() { return end_; }

  struct InsertResult {
    String str;
    ID id;
    CommandPtr command;
  };
  InsertResult Insert(SitePtr site, Char c, ID after) {
    const CharInfo* aftc = avl_.Lookup(after);
    assert(aftc);
    ID before = aftc->next;
    ID new_id = site->GenerateID();
    CommandPtr command(new InsertCommand(new_id, c, after, before));
    String str = Integrate(command);
    return InsertResult{str, new_id, std::move(command)};
  }

  struct RemoveResult {
    String str;
    CommandPtr command;
  };
  RemoveResult Remove(ID chr);

  String Integrate(const CommandPtr& command) {
    return command->Integrate(*this);
  }

  std::basic_string<Char> Render() {
    std::basic_string<Char> out;
    ID cur = avl_.Lookup(begin_)->next;
    while (cur != end_) {
      const CharInfo* c = avl_.Lookup(cur);
      out += c->chr;
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

  String(ID begin, ID end, functional_util::AVL<ID, CharInfo> avl)
      : begin_(begin), end_(end), avl_(avl) {}

  Site* root_site() {
    static SitePtr p = Site::Make();
    return p.get();
  }

  String IntegrateInsert(ID id, Char c, ID after, ID before) {
    const CharInfo* caft = avl_.Lookup(after);
    const CharInfo* cbef = avl_.Lookup(before);
    assert(caft != nullptr);
    assert(cbef != nullptr);
    if (caft->next == before) {
      return String(
          begin_, end_,
          avl_.Add(after, CharInfo{caft->visible, caft->chr, id, caft->prev,
                                   caft->after, caft->before})
              .Add(id, CharInfo{true, c, before, after, after, before})
              .Add(before, CharInfo{cbef->visible, cbef->chr, cbef->next, id,
                                    cbef->after, cbef->before}));
    }
    std::vector<ID> L;
    L.push_back(after);
    ID n = caft->next;
    do {
      const CharInfo* cn = avl_.Lookup(n);
      assert(cn != nullptr);
      // TODO(ctiller) - check if we should search back/forward here
      if (cn->before == before && cn->after == after) {
        L.push_back(n);
      }
      n = cn->next;
    } while (n != before);
    L.push_back(before);
    size_t i = 1;
    for (; i < L.size() - 1 && L[i] < id; i++)
      ;
    return IntegrateInsert(id, c, L[i - 1], L[i]);
  }

  class InsertCommand final : public Command {
   public:
    InsertCommand(ID id, Char c, ID after, ID before)
        : id_(id), c_(c), after_(after), before_(before) {}

    String Integrate(String s) {
      return s.IntegrateInsert(id_, c_, after_, before_);
    }

   private:
    ID id_;
    Char c_;
    ID after_;
    ID before_;
  };

  ID begin_;
  ID end_;
  functional_util::AVL<ID, CharInfo> avl_;
};

}  // namespace woot
