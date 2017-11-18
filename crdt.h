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

#include <stdlib.h>
#include <atomic>
#include <memory>
#include <tuple>
#include <vector>

class Site;

typedef uint64_t ID;

class Site {
 public:
  Site() : id_(id_gen_.fetch_add(1, std::memory_order_relaxed)) {
    if (id_ == 0) abort();
  }

  Site(const Site&) = delete;
  Site& operator=(const Site&) = delete;

  ID GenerateID() {
    return (clock_.fetch_add(1, std::memory_order_relaxed) << 16) | id_;
  }

  uint64_t site_id() const { return id_; }

 private:
  Site(uint16_t id) : id_(id) {}
  const uint16_t id_;
  std::atomic<uint64_t> clock_{0};
  static std::atomic<uint16_t> id_gen_;
};

template <class Derived>
class CRDT {
 public:
  class Command {
   public:
    Command(ID id) : id_(id) {}
    virtual ~Command() {}

    ID id() const { return id_; }

    virtual std::unique_ptr<Command> Clone() = 0;

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

      CommandPtr Clone() override {
        return CommandPtr(new Impl(this->id(), F(f_)));
      }

     private:
      Derived Integrate(Derived s) override { return f_(s, this->id()); }

      F f_;
    };
    buf->emplace_back(new Impl(id, std::move(f)));
    return id;
  }
};
