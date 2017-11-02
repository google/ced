config_setting(
    name = "linux_x86_64",
    values = {"cpu": "k8"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "darwin",
    values = {"cpu": "darwin"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "darwin_x86_64",
    values = {"cpu": "darwin_x86_64"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "iphonesdk",
    values = {"define": "IPHONE_SDK=1"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "freebsd",
    values = {"cpu": "freebsd"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "windows",
    values = {"cpu": "x64_windows"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "windows_msvc",
    values = {"cpu": "x64_windows_msvc"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "windows_msys",
    values = {"cpu": "x64_windows_msys"},
    visibility = ["//visibility:public"],
    )

config_setting(
    name = "arm",
    values = {"cpu": "arm"},
    visibility = ["//visibility:public"],
    )


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
  deps = [":avl", ":token_type"]
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
    ":terminal_collaborator",
    ":clang_format_collaborator",
    ":libclang_collaborator"
  ],
  linkopts = ["-lcurses", "-lpthread"]
)

cc_library(
  name = "token_type",
  hdrs = ["token_type.h"]
)

cc_library(
  name = "buffer",
  srcs = ["buffer.cc", "io_collaborator.cc"],
  hdrs = ["buffer.h", "io_collaborator.h"],
  deps = [
    ":woot",
    ":log",
    ":wrap_syscall",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/time",
    "@com_google_absl//absl/types:any",
  ]
)

cc_test(
  name = "buffer_test",
  srcs = ["buffer_test.cc"],
  deps = [":buffer", "@com_google_googletest//:gtest_main"]
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
    "@pugixml//:pugixml"
  ],
)

cc_library(
  name = "libclang_collaborator",
  srcs = ["libclang_collaborator.cc"],
  hdrs = ["libclang_collaborator.h"],
  deps = [
    ":buffer",
    ":log",
    ] + select({
        ":linux_x86_64": ["@clang_linux//:libclang"],
        ":darwin": ["@clang_mac//:libclang"],
    })
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
