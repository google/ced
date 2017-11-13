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
git_repository(
  name="com_google_absl",
  commit="6cf9c731027f4d8aebe3c60df8e64317e6870870",
  remote="https://github.com/abseil/abseil-cpp.git"
)

# GoogleTest/GoogleMock framework. Used by most unit-tests.
http_archive(
     name = "com_google_googletest",
     urls = ["https://github.com/google/googletest/archive/master.zip"],
     strip_prefix = "googletest-master",
)

# CCTZ (Time-zone framework).
http_archive(
    name = "com_googlesource_code_cctz",
    urls = ["https://github.com/google/cctz/archive/master.zip"],
    strip_prefix = "cctz-master",
)

new_git_repository(
  name="yaml",
  commit="beb44b872c07c74556314e730c6f20a00b32e8e5",
  remote="https://github.com/jbeder/yaml-cpp.git",
  build_file_content="""
cc_library(
  name="yaml",
  hdrs=glob(["include/**/*.h", "include/*.h"]),
  srcs=glob(["src/**/*.h", "src/*.h", "src/**/*.cpp", "src/*.cpp"]),
  includes=["include"],
  visibility=["//visibility:public"]
)
  """
)

new_http_archive(
  name="pugixml",
  urls = ["https://github.com/zeux/pugixml/releases/download/v1.8.1/pugixml-1.8.1.tar.gz"],
  strip_prefix = "pugixml-1.8",
  build_file_content="""
cc_library(
  name="pugixml",
  hdrs=["src/pugixml.hpp"],
  srcs=["src/pugiconfig.hpp", "src/pugixml.cpp"],
  visibility=["//visibility:public"]
)
  """
)

new_http_archive(
  name="clang_linux",
  urls = ["http://releases.llvm.org/5.0.0/clang+llvm-5.0.0-linux-x86_64-ubuntu14.04.tar.xz"],
  strip_prefix = "clang+llvm-5.0.0-linux-x86_64-ubuntu14.04",
  build_file = "BUILD.clang.linux"
)

new_http_archive(
  name="clang_mac",
  urls = ["http://releases.llvm.org/5.0.0/clang+llvm-5.0.0-x86_64-apple-darwin.tar.xz"],
  strip_prefix = "clang+llvm-5.0.0-x86_64-apple-darwin",
  build_file = "BUILD.clang.mac"
)

http_archive(
    name = "com_googlesource_code_re2",
    urls = ['https://github.com/google/re2/archive/2017-11-01.tar.gz'],
    strip_prefix = 're2-2017-11-01'
)

new_http_archive(
  name="one_dark",
  urls = ["https://github.com/andresmichel/one-dark-theme/archive/1.3.6.tar.gz"],
  strip_prefix = "one-dark-theme-1.3.6",
  build_file_content="""
filegroup(
  name="theme",
  srcs=["One Dark.tmTheme"],
  visibility=["//visibility:public"],
)
  """
)

new_http_archive(
  name="material",
  urls = ["https://github.com/equinusocio/material-theme/archive/v4.1.2.tar.gz"],
  strip_prefix = "material-theme-4.1.2",
  build_file_content="""
filegroup(
  name="theme",
  srcs=["schemes/Material-Theme.tmTheme"],
  visibility=["//visibility:public"],
)
  """
)

new_http_archive(
  name='rhea',
  urls=['https://github.com/Nocte-/rhea/archive/0.2.4.tar.gz'],
  strip_prefix='rhea-0.2.4',
  build_file_content="""
cc_library(
  name='rhea',
  srcs=glob(['rhea/*.cpp']),
  hdrs=glob(['rhea/*.hpp']),
  visibility=["//visibility:public"],
)
  """
)

new_git_repository(
  name='layout',
  commit="cad4eb645163d66707eb573ed63e34feb4311c32",
  remote="https://github.com/randrew/layout.git",
  build_file_content="""
genrule(
  name="layout_cc",
  outs = ['layout.cc'],
  cmd = 'echo -e "#define LAY_IMPLEMENTATION\\n#include \\\\"layout.h\\\\"\\n" > $(OUTS)'
)
cc_library(
  name="layout",
  hdrs=['layout.h'],
  srcs=['layout.cc'],
  visibility=["//visibility:public"]
)
  """
)
