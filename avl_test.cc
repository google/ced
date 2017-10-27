#include "avl.h"
#include <gtest/gtest.h>

using namespace functional_util;

TEST(AvlTest, NoOp) { AVL<int, int> avl; }

TEST(AvlTest, Lookup) {
  auto avl = AVL<int, int>().Add(1, 42);
  EXPECT_EQ(nullptr, avl.Lookup(2));
  EXPECT_EQ(42, *avl.Lookup(1));
}
