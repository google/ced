#include "wrap_syscall.h"
#include "gtest/gtest.h"

TEST(WrapSyscall, Succeeds) {
  EXPECT_EQ(42, WrapSyscall("test", []() { return 42; }));
}

TEST(WrapSyscall, Fails) {
  bool caught = false;
  try {
    WrapSyscall("test", []() {
      errno = EAGAIN;
      return -1;
    });
  } catch (std::exception& e) {
    caught = true;
  }
  EXPECT_TRUE(caught);
}
