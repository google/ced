#pragma once

#include <stdint.h>
#include <memory>
#include <tuple>

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

  SitePtr Make() { return SitePtr(new Site); }

 private:
  Site() {}
  std::atomic<uint64_t> clock_{0};
};

template <class Char>
class String {
 public:
  class Command {
   public:
    virtual ~Command() {}

   private:
    friend class String;
    virtual String Integrate(String string) = 0;
  };
  typedef std::unique_ptr<Command> CommandPtr;

  struct InsertResult {
    String str;
    ID id;
    CommandPtr command;
  };
  InsertResult Insert(Site* site, Char c, ID after) {
    class InsertCommand : public Command {
     public:
     private:
    };
    CharInfo* aftc = avl_.Lookup(after);
  }

  struct RemoveResult {
    String str;
    CommandPtr command;
  };
  RemoveResult Remove(ID chr);

  String Integrate(CommandPtr command) { return command->Integrate(*this); }

 private:
  struct CharInfo {
    bool visible;
    Char chr;
    ID next;
    ID prev;
  };

  functional_util::AVL<ID, CharInfo> avl_;
};

}  // namespace woot
