#include "cppfilt.h"
#include <gtest/gtest.h>

TEST(CppFilt, Simple) { EXPECT_EQ(cppfilt("_Z4testv"), "test()"); }
