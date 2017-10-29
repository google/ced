#pragma once

#include <assert.h>
#include <stdint.h>
#include <map>
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

  static ID Begin() {
    static ID begin_id = root_site()->GenerateID();
    return begin_id;
  }
  static ID End() {
    static ID end_id = root_site()->GenerateID();
    return end_id;
  }

  struct InsertResult {
    String str;
    CommandPtr command;
  };
  static CommandPtr MakeRawInsert(SitePtr site, Char c, ID after, ID before) {
    ID new_id = site->GenerateID();
    return CommandPtr(new InsertCommand(new_id, c, after, before));
  }
  CommandPtr MakeInsert(SitePtr site, Char c, ID after) {
    return MakeRawInsert(site, c, after, avl_.Lookup(after)->next);
  }
  InsertResult Insert(SitePtr site, Char c, ID after) {
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

  String(functional_util::AVL<ID, CharInfo> avl) : avl_(avl) {}

  static Site* root_site() {
    static SitePtr p = Site::Make();
    return p.get();
  }

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

  functional_util::AVL<ID, CharInfo> avl_;
};

}  // namespace woot
