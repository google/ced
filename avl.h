#pragma once

#include <algorithm>
#include <memory>

template <class K, class V>
class AVL {
 public:
  AVL() {}

  AVL Add(K key, V value) const {
    return AVL(AddKey(root_, std::move(key), std::move(value)));
  }
  AVL Remove(const K &key) const { return AVL(RemoveKey(root_, key)); }
  const V *Lookup(const K &key) const {
    NodePtr n = Get(root_, key);
    return n ? &n->value : nullptr;
  }

  bool Empty() const { return root_ == nullptr; }

  template <class F>
  void ForEach(F &&f) {
    ForEachImpl(root_.get(), std::forward<F>(f));
  }

 private:
  struct Node;
  typedef std::shared_ptr<Node> NodePtr;
  struct Node : public std::enable_shared_from_this<Node> {
    Node(K k, V v, NodePtr l, NodePtr r, long h)
        : key(std::move(k)),
          value(std::move(v)),
          left(std::move(l)),
          right(std::move(r)),
          height(h) {}
    const K key;
    const V value;
    const NodePtr left;
    const NodePtr right;
    const long height;
  };
  NodePtr root_;

  AVL(NodePtr root) : root_(std::move(root)) {}

  template <class F>
  void ForEachImpl(Node *n, F &&f) {
    if (n == nullptr) return;
    ForEachImpl(n->left, std::forward(f));
    f(const_cast<const K &>(n->key), const_cast<const V &>(n->value));
    ForEachImpl(n->right, std::forward(f));
  }

  static long Height(const NodePtr &n) { return n ? n->height : 0; }

  static NodePtr MakeNode(K key, V value, const NodePtr &left,
                          const NodePtr &right) {
    return std::make_shared<Node>(std::move(key), std::move(value), left, right,
                                  1 + std::max(Height(left), Height(right)));
  }

  static NodePtr Get(const NodePtr &node, const K &key) {
    if (node == nullptr) {
      return nullptr;
    }

    if (node->key > key) {
      return Get(node->left, key);
    } else if (node->key < key) {
      return Get(node->right, key);
    } else {
      return node;
    }
  }

  static NodePtr RotateLeft(K key, V value, const NodePtr &left,
                            const NodePtr &right) {
    return MakeNode(
        right->key, right->value,
        MakeNode(std::move(key), std::move(value), left, right->left),
        right->right);
  }

  static NodePtr RotateRight(K key, V value, const NodePtr &left,
                             const NodePtr &right) {
    return MakeNode(
        left->key, left->value, left->left,
        MakeNode(std::move(key), std::move(value), left->right, right));
  }

  static NodePtr RotateLeftRight(K key, V value, const NodePtr &left,
                                 const NodePtr &right) {
    /* rotate_right(..., rotate_left(left), right) */
    return MakeNode(
        left->right->key, left->right->value,
        MakeNode(left->key, left->value, left->left, left->right->left),
        MakeNode(std::move(key), std::move(value), left->right->right, right));
  }

  static NodePtr RotateRightLeft(K key, V value, const NodePtr &left,
                                 const NodePtr &right) {
    /* rotate_left(..., left, rotate_right(right)) */
    return MakeNode(
        right->left->key, right->left->value,
        MakeNode(std::move(key), std::move(value), left, right->left->left),
        MakeNode(right->key, right->value, right->left->right, right->right));
  }

  static NodePtr Rebalance(K key, V value, const NodePtr &left,
                           const NodePtr &right) {
    switch (Height(left) - Height(right)) {
      case 2:
        if (Height(left->left) - Height(left->right) == -1) {
          return RotateLeftRight(std::move(key), std::move(value), left, right);
        } else {
          return RotateRight(std::move(key), std::move(value), left, right);
        }
      case -2:
        if (Height(right->left) - Height(right->right) == 1) {
          return RotateRightLeft(std::move(key), std::move(value), left, right);
        } else {
          return RotateLeft(std::move(key), std::move(value), left, right);
        }
      default:
        return MakeNode(key, value, left, right);
    }
  }

  static NodePtr AddKey(const NodePtr &node, K key, V value) {
    if (!node) {
      return MakeNode(std::move(key), std::move(value), nullptr, nullptr);
    }
    if (node->key < key) {
      return Rebalance(node->key, node->value, node->left,
                       AddKey(node->right, std::move(key), std::move(value)));
    }
    if (key < node->key) {
      return Rebalance(node->key, node->value,
                       AddKey(node->left, std::move(key), std::move(value)),
                       node->right);
    }
    return MakeNode(std::move(key), std::move(value), node->left, node->right);
  }

  static NodePtr InOrderHead(NodePtr node) {
    while (node->left != nullptr) {
      node = node->left;
    }
    return node;
  }

  static NodePtr InOrderTail(NodePtr node) {
    while (node->right != nullptr) {
      node = node->right;
    }
    return node;
  }

  static NodePtr RemoveKey(const NodePtr &node, const K &key) {
    if (node == nullptr) {
      return nullptr;
    }
    if (key < node->key) {
      return Rebalance(node->key, node->value, RemoveKey(node->left, key),
                       node->right);
    } else if (node->key < key) {
      return Rebalance(node->key, node->value, node->left,
                       RemoveKey(node->right, key));
    } else {
      if (node->left == nullptr) {
        return node->right;
      } else if (node->right == nullptr) {
        return node->left;
      } else if (node->left->height < node->right->height) {
        NodePtr h = InOrderHead(node->right);
        return Rebalance(h->key, h->value, node->left,
                         RemoveKey(node->right, h->key));
      } else {
        NodePtr h = InOrderTail(node->left);
        return Rebalance(h->key, h->value, RemoveKey(node->left, h->key),
                         node->right);
      }
    }
    abort();
  }
};
