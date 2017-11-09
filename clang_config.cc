// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "clang_config.h"
#include <sys/param.h>
#include <stdexcept>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "config.h"
#include "read.h"
#include "log.h"

Config<std::string> clang_version("project/clang-version");

static std::vector<absl::string_view> Path() {
  const char* path = getenv("PATH");
  return absl::StrSplit(path, ':');
}

static bool Exists(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  fclose(f);
  return true;
}

std::string ClangToolPath(const std::string& tool_name) {
  auto version = clang_version.get();
  if (!version.empty()) {
    auto path = Config<std::string>(absl::StrCat("clang/", version)).get();
    if (!path.empty()) {
      auto cmd = absl::StrCat(path, "/bin/", tool_name);
      if (Exists(cmd)) return cmd;
    }
    for (auto path : Path()) {
      auto cmd = absl::StrCat(path, "/", tool_name, "-", version);
      if (Exists(cmd)) return cmd;
    }
  }
  for (auto path : Path()) {
    auto cmd = absl::StrCat(path, "/", tool_name);
    if (Exists(cmd)) return cmd;
  }
  throw std::runtime_error(
      absl::StrCat("Clang tool '", tool_name, "' not found"));
}

std::string ClangLibPath(const std::string& lib_name) {
  auto version = clang_version.get();
  if (!version.empty()) {
    auto path = Config<std::string>(absl::StrCat("clang/", version)).get();
    if (!path.empty()) {
      auto lib = absl::StrCat(path, "/lib/", lib_name);
      if (Exists(lib)) return lib;
    }
    for (auto path : {"/lib", "/usr/lib", "/usr/local/lib"}) {
      auto lib = absl::StrCat(path, "/", lib_name, ".", version);
      if (Exists(lib)) return lib;
    }
  }
  for (auto path : {"/lib", "/usr/lib", "/usr/local/lib"}) {
    auto lib = absl::StrCat(path, "/", lib_name);
    if (Exists(lib)) return lib;
  }
  throw std::runtime_error(
      absl::StrCat("Clang lib '", lib_name, "' not found"));
}

static std::string ClangBuiltinSystemIncludePath() {
  std::string tool_path = ClangToolPath("clang++");
  return tool_path.substr(0, tool_path.rfind('/')) + "/../include";
}

void ClangCompileArgs(const std::string& filename, std::vector<std::string>* args) {
  args->push_back("-x");
  args->push_back("c++");
  args->push_back("-std=c++11");
//  args->push_back(absl::StrCat("-isystem=", ClangBuiltinSystemIncludePath()));
  
  char cwd[MAXPATHLEN];
  getcwd(cwd, sizeof(cwd));
  std::vector<absl::string_view> segments = absl::StrSplit(cwd, '/');
  while (!segments.empty()) {
    auto path = absl::StrJoin(segments, "/");
    try {
      auto clang_config = Read(absl::StrCat(path, "/.clang_complete"));
      for (auto arg : absl::StrSplit(clang_config, '\n')) {
        args->emplace_back(arg.data(), arg.length());
      }
      return;
    } catch (std::exception& e) {
      Log() << e.what();
    }
    segments.pop_back();
  }
}

std::string ClangCompileCommand(const std::string& filename,
                                const std::string& src_file,
                                const std::string& dst_file,
                                std::vector<std::string>* args) {
  args->push_back("-g");
  args->push_back("-O2");
  args->push_back("-DNDEBUG");

  ClangCompileArgs(filename, args);

  args->push_back("-c");
  args->push_back("-o");
  args->push_back(dst_file);
  args->push_back(src_file);

  return ClangToolPath("clang++");
}
