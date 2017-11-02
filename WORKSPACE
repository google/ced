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
  commit="09ee99a282ea003bbacedc22f70e61653ff6ab92",
  remote="https://github.com/arun11299/cpp-subprocess.git",
  build_file_content="""
cc_library(
  name="subprocess",
  hdrs=["subprocess.hpp"],
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
  name="clang_mac",
  urls = ["http://releases.llvm.org/5.0.0/clang+llvm-5.0.0-x86_64-apple-darwin.tar.xz"],
  strip_prefix = "clang+llvm-5.0.0-x86_64-apple-darwin",
  build_file = "BUILD.clang"
)
