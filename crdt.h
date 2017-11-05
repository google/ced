#pragma once

#include <tuple>
#include <atomic>
#include <vector>

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

template <class Derived>
class CRDT {
 public:
  class Command {
   public:
    Command(ID id) : id_(id) {}
    virtual ~Command() {}

    ID id() const { return id_; }

   private:
    friend class CRDT<Derived>;
    virtual Derived Integrate(Derived string) = 0;
    const ID id_;
  };
  typedef std::unique_ptr<Command> CommandPtr;
  typedef std::vector<CommandPtr> CommandBuf;

  Derived Integrate(const CommandPtr& command) {
    return command->Integrate(*static_cast<Derived*>(this));
  }

 protected:
  template <class F>
  static ID MakeCommand(CommandBuf* buf, ID id, F&& f) {
    class Impl final : public Command {
     public:
      Impl(ID id, F&& f) : Command(id), f_(std::move(f)) {}

     private:
      Derived Integrate(Derived s) override { return f_(s, this->id()); }

      F f_;
    };
    buf->emplace_back(new Impl(id, std::move(f)));
    return id;
  }
};

