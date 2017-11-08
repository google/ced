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
  name="subprocess",
  commit="70e9e6cc01ab6de692b8bd151d0c4bb168078644",
  remote="https://github.com/tsaarni/cpp-subprocess.git",
  build_file_content="""
cc_library(
  name="subprocess",
  hdrs=["include/subprocess.hpp"],
  visibility=["//visibility:public"]
)
  """
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

