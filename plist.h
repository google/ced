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
#include <unordered_map>
#include <vector>

namespace plist {

class Array;
class Dict;

class Node {
 public:
  virtual const std::string* AsString() const = 0;
  virtual const Array* AsArray() const = 0;
  virtual const Dict* AsDict() const = 0;

  int AsInt(int def) const {
    const std::string* s = AsString();
    if (s == nullptr) return def;
    int n;
    if (sscanf(s->c_str(), "%d", &n) == 1) return n;
    return def;
  }
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

  void AddNode(NodePtr&& node) { nodes_.emplace_back(std::move(node)); }

  std::vector<NodePtr>* mutable_nodes() { return &nodes_; }

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

  const Node* Get(const std::string& key) const {
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return nullptr;
    return it->second.get();
  }

  template <class T, class... Args>
  T* Add(const std::string& key, Args&&... args) {
    T* t = new T(std::forward<Args>(args)...);
    nodes_.emplace(std::make_pair(key, NodePtr(t)));
    return t;
  }

  void AddNode(const std::string& key, NodePtr&& node) {
    nodes_.emplace(std::make_pair(key, std::move(node)));
  }

 private:
  std::unordered_map<std::string, NodePtr> nodes_;
};

NodePtr Parse(const std::string& buffer);
NodePtr Load(const std::string& filename);

}  // namespace plist
