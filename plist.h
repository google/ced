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

#include <memory>
#include <string>

namespace plist {

class Array;
class Dict;

class Node {
 public:
  virtual const std::string* AsString() const = 0;
  virtual const Array* AsArray() const = 0;
  virtual const Dict* AsDict() const = 0;
};

typedef std::unique_ptr<const Node> NodePtr;

class String final : public Node {
 public:
  explicit String(const std::string& s) : s_(s) {}

  const std::string* AsString() const override { return &s_; }
  const Array* AsArray() const override { return nullptr; }
  const Dict* AsDict() const override { return nullptr; }

 private:
  std::string s_;
};

class Array final : public Node {
 public:
  const std::string* AsString() const override { return nullptr; }
  const Array* AsArray() const override { return this; }
  const Dict* AsDict() const override { return nullptr; }

  std::vector<NodePtr>::const_iterator begin() const { return nodes_.begin(); }
  std::vector<NodePtr>::const_iterator end() const { return nodes_.end(); }

  template <class T, class... Args>
  T* Add(Args&&... args) {
    T* t = new T(std::forward<Args>(args)...);
    nodes_.emplace_back(t);
    return t;
  }

 private:
  std::vector<NodePtr> nodes_;
};

class Dict final : public Node {
 public:
  const std::string* AsString() const override { return nullptr; }
  const Array* AsArray() const override { return nullptr; }
  const Dict* AsDict() const override { return this; }

  std::unordered_map<std::string, NodePtr>::const_iterator begin() const {
    return nodes_.begin();
  }
  std::unordered_map<std::string, NodePtr>::const_iterator end() const {
    return nodes_.end();
  }

  template <class T, class... Args>
  T* Add(const std::string& key, Args&&... args) {
    T* t = new T(std::forward<Args>(args)...);
    nodes_.emplace(std::make_pair(key, NodePtr(t)));
    return t;
  }

 private:
  std::unordered_map<std::string, NodePtr> nodes_;
};

NodePtr Parse(const std::string& buffer);
NodePtr Load(const std::string& filename);

}  // namespace plist
