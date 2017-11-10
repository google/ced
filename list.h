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

template <class T>
class List {
 public:
  List() : root_(nullptr) {}

  List Push(const T& value) { return List(NodePtr(new Node{value, root_})); }

  template <class F>
  void ForEach(F&& f) {
    Node* n = root_.get();
    while (n) {
      f(n->value);
      n = n->next.get();
    }
  }

  bool operator<(const List& rhs) const {
    return Compare(root_.get(), rhs.root_.get()) < 0;
  }

  bool Empty() const { return !root_; }
  const T& Head() const { return root_->value; }
  List Tail() const { return List(root_->next); }

 private:
  struct Node;
  typedef std::shared_ptr<Node> NodePtr;
  struct Node {
    T value;
    NodePtr next;
  };
  NodePtr root_;

  static int Compare(Node* left, Node* right) {
    if (left == nullptr) {
      return right == nullptr ? 0 : -1;
    } else if (right == nullptr) {
      return 1;
    } else if (left->value < right->value) {
      return -1;
    } else if (left->value > right->value) {
      return 1;
    } else {
      return Compare(left->next.get(), right->next.get());
    }
  }

  List(NodePtr n) : root_(n) {}
};
