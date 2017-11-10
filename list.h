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
  const T& Head() { return root_->value; }

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
