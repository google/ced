cc_library(
  name = "avl",
  hdrs = ["avl.h"]
)

cc_test(
  name = "avl_test",
  srcs = ["avl_test.cc"],
  deps = [":avl", "@com_google_googletest//:gtest_main"]
)

cc_library(
  name = "woot",
  hdrs = ["woot.h"],
  deps = [":avl"]
)

cc_test(
  name = "woot_test",
  srcs = ["woot_Test.cc"],
  deps = [":woot", "@com_google_googletest//:gtest_main"]
)

cc_binary(
  name = "ced",
  srcs = ["main.cc"],
  deps = ["@tl//:fullscreen"]
)
