# Copyright 2017 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
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
  name = "list",
  hdrs = ["list.h"]
)

cc_library(
  name = "crdt",
  hdrs = ["crdt.h"],
)

cc_library(
  name = "woot",
  hdrs = ["woot.h"],
  srcs = ["woot.cc"],
  deps = [":avl", ":crdt"]
)

cc_library(
  name = "umap",
  hdrs = ["umap.h"],
  deps = [":avl", ":crdt", ":log"]
)

cc_library(
  name = "uset",
  hdrs = ["uset.h"],
  deps = [":avl", ":crdt"]
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
    ":libclang_collaborator",
    ":godbolt_collaborator",
    ":config",
    ":terminal_color",
  ],
  linkopts = ["-lcurses", "-lpthread", "-ldl"]
)

cc_library(
  name = "token_type",
  hdrs = ["token_type.h"],
  srcs = ["token_type.cc"],
  deps = [":list"]
)

cc_library(
  name = "side_buffer",
  hdrs = ["side_buffer.h"],
  srcs = ["side_buffer.cc"],
  deps = [":token_type"],
)

cc_library(
  name = "buffer",
  srcs = ["buffer.cc", "io_collaborator.cc", "diagnostic.cc"],
  hdrs = ["buffer.h", "io_collaborator.h", "diagnostic.h"],
  deps = [
    ":woot",
    ":umap",
    ":uset",
    ":log",
    ":wrap_syscall",
    ":token_type",
    ":side_buffer",
    ":temp_file",
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
  deps = [":buffer", ":log", ":terminal_color", "@com_google_absl//absl/time"],
)

cc_library(
  name = "godbolt_collaborator",
  srcs = ["godbolt_collaborator.cc"],
  hdrs = ["godbolt_collaborator.h"],
  deps = [
    ":buffer",
    ":log",
    ":clang_config",
    ":temp_file",
    ":asm_parser",
    ":run",
  ],
)

cc_library(
  name = "clang_format_collaborator",
  srcs = ["clang_format_collaborator.cc"],
  hdrs = ["clang_format_collaborator.h"],
  deps = [
    ":buffer",
    ":log",
    ":config",
    ":clang_config",
    ":run",
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
    ":clang_config",
    "//libclang:libclang",
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
  name = 'config',
  srcs = ['config.cc'],
  hdrs = ['config.h'],
  deps = [
    '@yaml//:yaml',
    '@com_google_absl//absl/synchronization',
    '@com_google_absl//absl/strings',
    ":log",
  ],
)

cc_library(
  name = 'clang_config',
  srcs = ['clang_config.cc'],
  hdrs = ['clang_config.h'],
  deps = [
      '@com_googlesource_code_re2//:re2',
      '@com_google_absl//absl/strings',
      ':config',
      ':read',
      ':run',
  ],
)

cc_library(
    name = "temp_file",
    hdrs = ['temp_file.h'],
    deps = ['wrap_syscall']
)

cc_test(
    name = "asm_parser_test",
    srcs = ["asm_parser_test.cc"],
    deps = [":asm_parser", "@com_google_googletest//:gtest_main"]
)

cc_library(
    name = "asm_parser",
    srcs = ["asm_parser.cc"],
    hdrs = ["asm_parser.h"],
    deps = [
        "@com_google_absl//absl/strings",
        '@com_googlesource_code_re2//:re2',
        ":log",
        ":cppfilt",
    ]
)

cc_test(
    name = "cppfilt_test",
    srcs = ["cppfilt_test.cc"],
    deps = [":cppfilt", "@com_google_googletest//:gtest_main"]
)

cc_library(
    name = "cppfilt",
    srcs = ["cppfilt.cc"],
    hdrs = ["cppfilt.h"],
    deps = [
        "@com_google_absl//absl/strings",
        ":run"
    ]
)

cc_library(
    name = "run",
    srcs = ["run.cc"],
    hdrs = ["run.h"],
    deps = [":wrap_syscall", ":log", "@com_google_absl//absl/strings"]
)

cc_library(
    name = "read",
    srcs = ["read.cc"],
    hdrs = ["read.h"],
    deps = [":wrap_syscall"],
)

cc_library(
  name = "project",
  srcs = ["project.cc"],
  hdrs = ["project.h"],
  deps = ["@com_google_absl//absl/strings"]
)

cc_binary(
  name = "projinf",
  srcs = ["projinf.cc"],
  deps = [":project"],
)

cc_library(
  name = "plist",
  srcs = ["plist.cc"],
  hdrs = ["plist.h"],
  deps = [
    "@pugixml//:pugixml"
  ]
)

cc_test(
  name = "plist_test",
  srcs = ["plist_test.cc"],
  deps = [
    ":plist",
    "@com_google_googletest//:gtest_main",
  ]
)

py_binary(
  name = "file2c",
  srcs = ["file2c.py"]
)

genrule(
  name = "default_theme_src",
  srcs = ["@one_dark//:one_dark"],
  outs = ['default_theme.cc'],
  tools = [':file2c'],
  cmd = './$(location :file2c) $(OUTS) default_theme $(locations @one_dark//:one_dark)'
)

cc_library(
  name = "default_theme",
  srcs = ['default_theme.cc'],
  hdrs = ['default_theme.h']
)

cc_library(
  name = "theme",
  srcs = ['theme.cc'],
  hdrs = ['theme.h'],
  deps = [
    ':read',
    ':plist',
    ':default_theme',
    ':token_type',
    '@com_google_absl//absl/types:optional'
  ]
)

cc_library(
  name = "terminal_color",
  srcs = ["terminal_color.cc"],
  hdrs = ["terminal_color.h"],
  deps = [
      ":theme",
  ]
)

cc_test(
  name = "theme_test",
  srcs = ["theme_test.cc"],
  deps = [":theme", "@com_google_googletest//:gtest_main"]
)
