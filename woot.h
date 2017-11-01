#pragma once

#include <assert.h>
#include <stdint.h>
#include <atomic>
#include <map>
#include <memory>
#include <string>
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

class String {
 public:
  String() {
    avl_ =
        avl_.Add(Begin(), CharInfo{false, char(), End(), End(), End(), End()})
            .Add(End(),
                 CharInfo{false, char(), Begin(), Begin(), Begin(), Begin()});
  }

  static ID Begin() { return begin_id_; }
  static ID End() { return end_id_; }

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

  static CommandPtr MakeRawInsert(Site* site, char c, ID after, ID before) {
    return MakeCommand(site->GenerateID(), [c, after, before](String s, ID id) {
      return s.IntegrateInsert(id, c, after, before);
    });
  }
  CommandPtr MakeInsert(Site* site, char c, ID after) const {
    return MakeRawInsert(site, c, after, avl_.Lookup(after)->next);
  }

  CommandPtr MakeRemove(ID chr) const {
    return MakeCommand(chr, [](String s, ID id) {
      return s.IntegrateRemove(id);
    });
  }

#if 0
  CommandPtr MakeAnnotate(Site* site, ID chr, absl::any annotation) {
    return MakeCommand(site->GenerateID(), [chr, annotation](String s, ID id) {
      return s.IntegrateAnnotate(id, chr, annotation);
    }
  }

  CommandPtr MakeMystify(ID annotation) {
    return MakeCommand(annotation, [](String s, ID id) {
      return s.IntegrateMystify(id);
    }
  }
#endif

  String Integrate(const CommandPtr& command) {
    return command->Integrate(*this);
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
    bool visible;
    char chr;
    ID next;
    ID prev;
    ID after;
    ID before;
//    AVL<ID, absl::any> annotations;
  };

  String(AVL<ID, CharInfo> avl) : avl_(avl) {}

  String IntegrateRemove(ID id) {
    const CharInfo* cdel = avl_.Lookup(id);
    if (!cdel->visible) return *this;
    return String(
        avl_.Add(id, CharInfo{false, cdel->chr, cdel->next, cdel->prev,
                              cdel->after, cdel->before}));
  }

  String IntegrateInsert(ID id, char c, ID after, ID before) {
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

  template <class F>
  static CommandPtr MakeCommand(ID id, F&& f) {
    class Impl final : public Command {
     public:
      Impl(ID id, F&& f) : Command(id), f_(std::move(f)) {}

     private:
      String Integrate(String s) override {
        return f_(s, id());
      }

      F f_;
    };
    return CommandPtr(new Impl(id, std::move(f)));
  }
  
  AVL<ID, CharInfo> avl_;
  AVL<ID, ID> annotation_chars_;
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
    const char value() const { return cur_->chr; }

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
