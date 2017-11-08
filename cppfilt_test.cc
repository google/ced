#include <gtest/gtest.h>
#include "cppfilt.h"

TEST(CppFilt, Simple) {
  EXPECT_EQ(cppfilt("_Z4testv"), "test()");
}

