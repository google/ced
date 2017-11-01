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
  srcs = ["woot.cc"],
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
  deps = [
    ":buffer",
    ":io_collaborator",
    ":terminal_collaborator",
    ":clang_format_collaborator"
  ],
  linkopts = ["-lcurses", "-lpthread"]
)

cc_library(
  name = "buffer",
  srcs = ["buffer.cc"],
  hdrs = ["buffer.h"],
  deps = [
    ":woot",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/types:any"]
)

cc_test(
  name = "buffer_test",
  srcs = ["buffer_test.cc"],
  deps = [":buffer", "@com_google_googletest//:gtest_main"]
)

cc_library(
  name = "io_collaborator",
  srcs = ["io_collaborator.cc"],
  hdrs = ["io_collaborator.h"],
  deps = [":buffer", ":wrap_syscall"],
)

cc_library(
  name = "terminal_collaborator",
  srcs = ["terminal_collaborator.cc"],
  hdrs = ["terminal_collaborator.h"],
  deps = [":buffer", ":log"],
)

cc_library(
  name = "clang_format_collaborator",
  srcs = ["clang_format_collaborator.cc"],
  hdrs = ["clang_format_collaborator.h"],
  deps = [
    ":buffer",
    ":log",
    "@subprocess//:subprocess",
    "@pugixml//:pugixml",
    ":batching_function"
  ],
)

cc_library(
  name = 'wrap_syscall',
  hdrs = ['wrap_syscall.h'],
  srcs = ['wrap_syscall.cc'],
  deps = ['@com_google_absl//absl/strings']
)

cc_test(
  name = 'wrap_syscall_test',
  srcs = ['wrap_syscall_test.cc'],
  deps = [':wrap_syscall', '@com_google_googletest//:gtest_main']
)

cc_library(
  name = "log",
  srcs = ["log.cc"],
  hdrs = ["log.h"],
  deps = [":wrap_syscall"]
)

cc_test(
  name = "log_test",
  srcs = ["log_test.cc"],
  deps = [":log"]
)

cc_library(
  name = "batching_function",
  hdrs = ["batching_function.h"],
  deps = ["@com_google_absl//absl/synchronization", ":log"]
)
