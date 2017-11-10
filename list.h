#pragma once

template <class T>
class List {
 public:
  List() : root_(nullptr) {}

  List Push(const T& value) {
    return List(NodePtr(new Node{value, root_}));
  }

  template <class F>
  void ForEach(F&& f) {
    Node* n = root_.get();
    while (n) {
      f(n->value);
      n = n->next.get();
    }
  }

 private:
  typedef std::shared_ptr<Node> NodePtr;
  struct Node {
    T value;
    NodePtr next;
  };
  NodePtr root_;

  List(NodePtr n) : root_(n) {}
};

