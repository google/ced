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

load('//:rules.bzl', 'file2lib')
load("@compiledb//:aspects.bzl", "compilation_database")

compilation_database(
    name = "compilation_database",
    targets = [
        "//:ced",
    ],
)

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

SRC_HASH_CMD = 'echo "const char* ced_src_hash=\\"`cat $(SRCS) | %s`\\";" > $(OUTS)'
genrule(
  name = 'src_hash_gen',
  srcs = glob(['*', '**/*'], exclude=[
    'bazel-*/*',
    'bazel-*/**/*',
    '.*/*',
    '.*/**/*',
    '.*'
  ]),
  outs=['src_hash.cc'],
  cmd= select({
    '//:darwin': SRC_HASH_CMD % 'md5',
    '//:darwin_x86_64': SRC_HASH_CMD % 'md5',
    '//conditions:default': SRC_HASH_CMD % 'md5sum | awk \'{print $$1}\'',
  }),
)

cc_library(
  name = 'src_hash',
  srcs = ['src_hash.cc'],
  hdrs = ['src_hash.h'],
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

apple_binary(
  name = 'cedmac',
  deps = [
    ':ced_main_gui',
    ":gui_client",
  ],
  platform_type = 'macos',
  sdk_frameworks = [
    'CoreServices',
    'CoreAudio',
    'Cocoa',
    'AudioToolbox',
    'CoreVideo',
    'ForceFeedback',
    'IOKit',
    'Carbon',
    'OpenGL',
  ]
)

cc_binary(
  name = 'ced',
  srcs = ["main.cc"],
  copts = ['-DDEFAULT_MODE=\\"Curses\\"'],
  deps = [
    ":application",
    ":curses_client",
    ":peep_show",
    ":standard_project_types",
    ":standard_collaborator_types",
    "@com_github_gflags_gflags//:gflags",
  ],
  linkstatic = 1,
  linkopts = ["-lcurses", "-ldl"] + select({
    ':darwin': ['-framework CoreServices'],
    ':darwin_x86_64': ['-framework CoreServices'],
    '//conditions:default': ['-lpthread'],
  })
)

cc_library(
  name = "standard_collaborator_types",
  deps = [
    ":clang_format_collaborator",
    ":libclang_collaborator",
    ":godbolt_collaborator",
    ":fixit_collaborator",
    ":referenced_file_collaborator",
    ":io_collaborator",
    ":regex_highlight_collaborator",
  ]
)

cc_library(
  name = "selector",
  hdrs = ["selector.h"],
  srcs = ["selector.cc"],
)

cc_library(
  name = "buffer",
  srcs = ["buffer.cc"],
  hdrs = ["buffer.h", "content_latch.h"],
  deps = [
    ":annotated_string",
    ":log",
    ":selector",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/time",
    "@com_google_absl//absl/types:optional",
    "@boost//:filesystem",
  ]
)

cc_library(
  name = "io_collaborator",
  srcs = ["io_collaborator.cc"],
  deps = [
    ":temp_file",
    ":wrap_syscall",
    ":buffer"
  ],
  alwayslink = 1,
)

cc_test(
  name = "buffer_test",
  srcs = ["buffer_test.cc"],
  deps = [":buffer", "@com_google_googletest//:gtest_main"]
)

cc_library(
  name = "client_collaborator",
  srcs = ["client_collaborator.cc"],
  hdrs = ["client_collaborator.h"],
  deps = [
    ":buffer",
    ":log",
    ":render",
    ":editor",
    ":line_editor",
    "@com_google_absl//absl/time",
    "@com_github_gflags_gflags//:gflags",
  ],
)

cc_library(
  name = "regex_highlight_collaborator",
  srcs = ["regex_highlight_collaborator.cc"],
  deps = [
    ":buffer",
    '@com_googlesource_code_re2//:re2',
  ],
  alwayslink = 1,
)

cc_library(
  name = "godbolt_collaborator",
  srcs = ["godbolt_collaborator.cc"],
  deps = [
    ":buffer",
    ":log",
    ":clang_config",
    ":temp_file",
    ":asm_parser",
    ":run",
  ],
  alwayslink = 1,
)

cc_library(
  name = "clang_format_collaborator",
  srcs = ["clang_format_collaborator.cc"],
  deps = [
    ":buffer",
    ":log",
    ":config",
    ":clang_config",
    ":run",
    "@pugixml//:pugixml"
  ],
  alwayslink = 1,
)

cc_library(
  name = "fixit_collaborator",
  srcs = ["fixit_collaborator.cc"],
  deps = [
    ":buffer",
    ":log",
  ],
  alwayslink = 1
)

cc_library(
  name = "libclang_collaborator",
  srcs = ["libclang_collaborator.cc"],
  deps = [
    ":buffer",
    ":log",
    ":clang_config",
    "//libclang:libclang",
  ],
  alwayslink = 1,
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
  deps = [
    ":wrap_syscall",
    '@com_google_absl//absl/strings',
    "@com_google_absl//absl/time",
    "@com_github_gflags_gflags//:gflags",
  ]
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
    ':project',
    "@boost//:filesystem",
  ],
)

cc_library(
  name = 'clang_config',
  srcs = ['clang_config.cc'],
  hdrs = ['clang_config.h'],
  deps = [
      '@com_googlesource_code_re2//:re2',
      '@com_google_absl//absl/strings',
      ':compilation_database_h',
      ':config',
      ':read',
      ':run',
      ':project',
      "@boost//:filesystem",
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
    deps = [
      ":wrap_syscall",
      ":log",
      "@com_google_absl//absl/strings",
      "@boost//:filesystem",
    ]
)

cc_library(
    name = "read",
    srcs = ["read.cc"],
    hdrs = ["read.h"],
    deps = [":wrap_syscall", "@boost//:filesystem"],
)

cc_library(
  name = "project",
  srcs = ["project.cc"],
  hdrs = ["project.h"],
  deps = [
      "@com_google_absl//absl/strings",
      "@boost//:filesystem"
  ]
)

cc_library(
  name = 'bazel_project',
  srcs = ['bazel_project.cc'],
  alwayslink = 1,
  deps = [':project']
)

cc_library(
  name = 'compilation_database_h',
  hdrs = ['compilation_database.h'],
  srcs = ['compilation_database.cc'],
  deps = [
      '@json//:json',
      "@com_google_absl//absl/synchronization",
      "@com_google_absl//absl/strings",
      "@boost//:filesystem",
      ":read",
  ]
)

cc_library(
  name = 'clang_project',
  srcs = ['clang_project.cc'],
  alwayslink = 1,
  deps = [':project', 'compilation_database_h']
)

cc_library(
  name = 'ced_project',
  srcs = ['ced_project.cc'],
  alwayslink = 1,
  deps = [':project']
)

cc_library(
  name = 'standard_project_types',
  deps = [
    'bazel_project',
    'clang_project',
    'ced_project',
  ]
)

cc_binary(
  name = "projinf",
  srcs = ["projinf.cc"],
  deps = [":project", ":standard_project_types"],
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

file2lib(
  name = "default_theme",
  src = "@material//:theme",
)

cc_library(
  name = "theme",
  srcs = ['theme.cc'],
  hdrs = ['theme.h'],
  deps = [
    ':read',
    ':plist',
    ':default_theme',
    ':selector',
    ':log',
    ':attr',
    '@com_google_absl//absl/types:optional'
  ]
)

cc_library(
  name = "terminal_color",
  srcs = ["terminal_color.cc"],
  hdrs = ["terminal_color.h"],
  deps = [
      ":theme",
      ":log"
  ]
)

cc_test(
  name = "theme_test",
  srcs = ["theme_test.cc"],
  deps = [":theme", "@com_google_googletest//:gtest_main"]
)

cc_library(
  name = "attr",
  hdrs = ["attr.h"],
)

cc_library(
  name = "render",
  hdrs = ["render.h"],
  srcs = ["render.cc"],
  deps = [
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/types:optional",
    "@com_google_absl//absl/container:inlined_vector",
    "@com_google_absl//absl/time",
    "@rhea//:rhea",
    "@cityhash//:city",
    ":attr",
    ":log",
  ],
)

cc_library(
  name = "editor",
  hdrs = ["editor.h"],
  srcs = ["editor.cc"],
  deps = [":render", ":theme", ":buffer"],
)

cc_library(
  name = "line_editor",
  hdrs = ["line_editor.h"],
  srcs = ["line_editor.cc"],
  deps = [":selector"]
)

cc_library(
  name = "fswatch",
  hdrs = ["fswatch.h"],
  srcs = ["fswatch.cc"],
)

cc_binary(
  name = "fswatch_test",
  srcs = ["fswatch_test.cc"],
  deps = [":fswatch"],
  linkopts = ["-lpthread"]
)

cc_library(
  name = "referenced_file_collaborator",
  srcs = ["referenced_file_collaborator.cc"],
  deps = [
    ":buffer",
    ":log",
    ":fswatch",
  ],
  alwayslink = 1,
)

cc_binary(
  name = "bm_editor",
  srcs = ["bm_editor.cc"],
  deps = [":editor", "@benchmark//:benchmark"],
  linkopts = ["-lpthread"]
)

cc_library(
  name = "annotated_string",
  hdrs = ["annotated_string.h"],
  srcs = ["annotated_string.cc"],
  deps = [
    "//proto:annotation",
    ":avl",
    ":log",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/types:optional",
  ]
)

cc_library(
  name = "server",
  hdrs = ["server.h"],
  srcs = ["server.cc"],
  deps = [
      ":project",
      ":run",
      ":application",
      ":log",
      ":src_hash",
      "@grpc//:grpc++_unsecure",
      "//proto:project_service",
      "@com_google_absl//absl/synchronization",
      ":buffer",
  ],
)

cc_library(
  name = "application",
  hdrs = ["application.h"],
  srcs = ["application.cc"],
  deps = [":log"],
)

cc_library(
  name = "client",
  hdrs = ["client.h"],
  srcs = ["client.cc"],
  deps = [
    "//proto:project_service",
    ":server",
    ":src_hash",
    "@com_google_absl//absl/time",
    ":log",
  ]
)

cc_library(
  name = "curses_client",
  srcs = ["curses_client.cc"],
  deps = [
    ":buffer",
    ":client_collaborator",
    ":client",
    ":application",
    ":terminal_color",
  ],
  alwayslink = 1,
)

cc_library(
  name = "peep_show",
  srcs = ["peep_show.cc"],
  deps = [
    ":client",
    ":application",
  ],
  alwayslink = 1,
)

cc_library(
  name = "sdl_info",
  srcs = ["sdl_info.cc"],
  deps = [
    ":application",
    "@SDL2//:sdl2",
  ],
  alwayslink = 1,
)

genrule(
  name = "prep_fontconfig",
  srcs = ["@fontconfig//:conf"],
  outs = ["fontconfig.txt"],
  cmd = "python $(location merge_fontconfig_confs.py) $(SRCS) > $@",
  tools = ["merge_fontconfig_confs.py"]
)

file2lib(
  name = "fontconfig_conf_files",
  src = "fontconfig.txt",
)
