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
  srcs = ["woot_test.cc"],
  deps = [":woot", "@com_google_googletest//:gtest_main"]
)

cc_binary(
  name = "ced",
  srcs = ["main.cc"],
  deps = ["@tl//:fullscreen"]
)

cc_library(
  name = "buffer",
  srcs = ["buffer.cc"],
  hdrs = ["buffer.h"],
  deps = [":woot", "@com_google_absl//absl/synchronization"]
)

cc_test(
  name = "buffer_test",
  srcs = ["buffer_test.cc"],
  deps = [":buffer", "@com_google_googletest//:gtest_main"]
)
