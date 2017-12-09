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

workspace(name = "ced")

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

new_git_repository(
    name = "benchmark",
    commit="0c3ec998c4cc6b7bba7226a3b35a7917613b3802",
    remote="https://github.com/google/benchmark.git",
    build_file_content="""
cc_library(
    name = "benchmark",
    srcs = glob(["src/*.cc"]),
    hdrs = glob(["include/**/*.h", "src/*.h"]),
    includes = [
        "include", "."
    ],
    copts = [
        "-DHAVE_POSIX_REGEX"
    ],
    linkstatic = 1,
    visibility = [
        "//visibility:public",
    ],
)
    """
)

new_http_archive(
  name = "compiledb",
  urls = ['https://github.com/grailbio/bazel-compilation-database/archive/0.2.tar.gz'],
  strip_prefix = 'bazel-compilation-database-0.2',
  build_file_content = """
filegroup(
  name="aspects",
  srcs=["aspects.bzl"],
  visibility=["//visibility:public"],
)
  """
)

new_http_archive(
  name = "json",
  urls = ['https://github.com/nlohmann/json/archive/v2.1.1.tar.gz'],
  strip_prefix = 'json-2.1.1',
  build_file_content = """
cc_library(
  name = "json",
  hdrs = ['src/json.hpp'],
  visibility = ['//visibility:public'],
)
  """
)

http_archive(
  name = "com_google_protobuf",
  urls = ['https://github.com/google/protobuf/releases/download/v3.5.0/protobuf-all-3.5.0.tar.gz'],
  strip_prefix = "protobuf-3.5.0"
)

git_repository(
    name   = "com_github_gflags_gflags",
    #tag    = "v2.2.0",
    commit = "30dbc81fb5ffdc98ea9b14b1918bfe4e8779b26e", # v2.2.0 + fix of include path
    remote = "https://github.com/gflags/gflags.git"
)

#
#  boost
#

git_repository(
    name = "com_github_nelhage_boost",
    commit = 'd6446dc9de6e43b039af07482a9361bdc6da5237',
    remote = "https://github.com/nelhage/rules_boost",
)

load("@com_github_nelhage_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

#
# imgui
#

new_http_archive(
      name = "dear_imgui",
      urls = ['https://github.com/ocornut/imgui/archive/v1.52.tar.gz'],
      strip_prefix = 'imgui-1.52',
      build_file_content = """
cc_library(
  name = "imgui",
  srcs = glob(['*.cpp']),
  hdrs = glob(['*.h']),
)
      """
)

#
# Skia
#

git_repository(
  name = "skia",
  remote = "https://github.com/ctiller/skia.git",
  commit = "48450946e8163c2746562756e6aa82b76327670b",
)

new_http_archive(
    name = "jpeg",
    urls = [
        "https://mirror.bazel.build/github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.1.tar.gz",
        "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.1.tar.gz",
    ],
    sha256 = "c15a9607892113946379ccea3ca8b85018301b200754f209453ab21674268e77",
    strip_prefix = "libjpeg-turbo-1.5.1",
    build_file = "BUILD.libjpeg",
)

new_http_archive(
    name = "freetype",
    urls = [
        "https://download.savannah.gnu.org/releases/freetype/freetype-2.8.1.tar.bz2",
    ],
    strip_prefix = "freetype-2.8.1",
    build_file = "BUILD.freetype",
)

new_git_repository(
    name = "fontconfig",
    remote = "https://anongit.freedesktop.org/git/fontconfig",
    tag = "2.12.6",
    build_file = "BUILD.fontconfig"
)

new_git_repository(
    name = "sfntly",
    remote = "https://github.com/rillig/sfntly",
    commit = "71b36ba9b512ddae85be6f9a3f6a3e808ae3ba82",
    build_file = "BUILD.sfntly",
)

#git_repository(
#    name = "com_github_spinorx_icu",
#    remote = "https://github.com/spinorx/icu.git",
#    tag = "v59.1.use"
#)

new_http_archive(
    name = "icu",
    urls = [
        "http://download.icu-project.org/files/icu4c/60.1/icu4c-60_1-src.tgz"
    ],
    strip_prefix = "icu",
    build_file = "BUILD.icu",
)

new_http_archive(
    name = "libpng",
    urls = [
        "https://mirror.bazel.build/github.com/glennrp/libpng/archive/v1.2.53.tar.gz",
        "https://github.com/glennrp/libpng/archive/v1.2.53.tar.gz",
    ],
    sha256 = "716c59c7dfc808a4c368f8ada526932be72b2fcea11dd85dc9d88b1df1dfe9c2",
    strip_prefix = "libpng-1.2.53",
    build_file = "BUILD.libpng",
)

new_git_repository(
    name = "webp",
    remote = "https://chromium.googlesource.com/webm/libwebp.git",
    tag = "v0.6.1",
    build_file = "BUILD.libwebp",
)

new_http_archive(
    name = "expat",
    urls = [
        "https://github.com/libexpat/libexpat/releases/download/R_2_2_5/expat-2.2.5.tar.bz2"
    ],
    strip_prefix = "expat-2.2.5",
    build_file = "BUILD.expat",
)

new_http_archive(
    name = "nasm",
    urls = [
        "https://mirror.bazel.build/www.nasm.us/pub/nasm/releasebuilds/2.12.02/nasm-2.12.02.tar.bz2",
        "http://pkgs.fedoraproject.org/repo/pkgs/nasm/nasm-2.12.02.tar.bz2/d15843c3fb7db39af80571ee27ec6fad/nasm-2.12.02.tar.bz2",
    ],
    sha256 = "00b0891c678c065446ca59bcee64719d0096d54d6886e6e472aeee2e170ae324",
    strip_prefix = "nasm-2.12.02",
    build_file = "BUILD.nasm",
)

new_git_repository(
    name = "xmp_sdk",
    remote = "https://github.com/hfiguiere/exempi.git",
    commit = "b1859382628b5ba961548980e3b0725d6f934b20",
    build_file = "BUILD.xmp_sdk"
)

new_git_repository(
    name = "dng_sdk",
    remote = "https://github.com/ctiller/dng_sdk.git",
    commit = "679d38be04af4f6d1c35fa9e09fed96f291843bd",
    build_file = "BUILD.dng_sdk"
)

new_git_repository(
    name = 'piex',
    remote = 'https://github.com/google/piex.git',
    commit = '473434f2dd974978b329faf5c87ae8aa09a2714d',
    build_file = 'BUILD.piex'
)


#
# gRPC bits
#

git_repository(
  name = "grpc",
  remote = 'https://github.com/grpc/grpc.git',
  commit = 'de93112a3f70afa39d3e9aa87da165f9f737fdef'
)

new_http_archive(
    name = "com_github_madler_zlib",
    build_file = "BUILD.zlib",
    strip_prefix = "zlib-cacf7f1d4e3d44d871b605da3b647f07d718623f",
    url = "https://github.com/madler/zlib/archive/cacf7f1d4e3d44d871b605da3b647f07d718623f.tar.gz",
)

http_archive(
    name = "boringssl",
    # on the master-with-bazel branch
    url = "https://boringssl.googlesource.com/boringssl/+archive/886e7d75368e3f4fab3f4d0d3584e4abfc557755.tar.gz",
)

new_http_archive(
    name = "com_github_cares_cares",
    build_file = "BUILD.cares",
    strip_prefix = "c-ares-3be1924221e1326df520f8498d704a5c4c8d0cce",
    url = "https://github.com/c-ares/c-ares/archive/3be1924221e1326df520f8498d704a5c4c8d0cce.tar.gz",
)

new_local_repository(
    name = "cares_local_files",
    build_file = "BUILD.cares_local_files",
    path = 'third_party/cares'
)

# required binds...
bind(
    name = "protobuf",
    actual = "@com_google_protobuf//:protobuf",
)

bind(
    name = "protobuf_clib",
    actual = "@com_google_protobuf//:protoc_lib",
)

bind(
    name = "protocol_compiler",
    actual = "@com_google_protobuf//:protoc",
)

bind(
    name = "grpc_cpp_plugin",
    actual = "@grpc//:grpc_cpp_plugin",
)

bind(
    name = "grpc++",
    actual = "@grpc//:grpc++",
)

bind(
    name = "grpc++_codegen_proto",
    actual = "@grpc//:grpc++_codegen_proto",
)

bind(
    name = "protobuf_headers",
    actual = "@com_google_protobuf//:protobuf_headers",
)

bind(
    name = "zlib",
    actual = "@com_github_madler_zlib//:z",
)

bind(
    name = "libssl",
    actual = "@boringssl//:ssl",
)

bind(
    name = "cares",
    actual = "@com_github_cares_cares//:ares",
)

bind(
    name = "nanopb",
    actual = "@grpc//third_party/nanopb",
)
