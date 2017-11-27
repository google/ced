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
#include <boost/filesystem.hpp>
#include <stdexcept>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "compilation_database.h"
#include "config.h"
#include "log.h"
#include "project.h"
#include "re2/re2.h"
#include "read.h"
#include "run.h"

#define PLAT_LIB_PFX "lib"
#if defined(__APPLE__)
#define PLAT_LIB_SFX ".dylib"
#else
#define PLAT_LIB_SFX ".so"
#endif

static std::vector<boost::filesystem::path> Path() {
  const char* path = getenv("PATH");
  std::vector<boost::filesystem::path> out;
  for (auto p : absl::StrSplit(path, ':')) {
    out.emplace_back(std::string(p.data(), p.length()));
  }
  return out;
}

boost::filesystem::path ClangToolPath(Project* project,
                                      const std::string& tool_name) {
  Config<std::string> clang_version(project, "project.clang-version");
  ConfigMap<std::string, boost::filesystem::path> clang_location(
      project, "clang.location");
  auto version = clang_version.get();
  if (!version.empty()) {
    auto path = clang_location.get(version);
    if (!path.empty()) {
      auto cmd = path / "bin" / tool_name;
      if (boost::filesystem::exists(cmd)) return cmd;
    }
    for (auto path : Path()) {
      auto cmd = path / absl::StrCat(tool_name, "-", version);
      if (boost::filesystem::exists(cmd)) return cmd;
    }
  }
  for (auto path : Path()) {
    auto cmd = path / tool_name;
    if (boost::filesystem::exists(cmd)) return cmd;
  }
  throw std::runtime_error(
      absl::StrCat("Clang tool '", tool_name, "' not found"));
}

boost::filesystem::path ClangLibPath(Project* project,
                                     const std::string& lib_base) {
  Config<std::string> clang_version(project, "project.clang-version");
  ConfigMap<std::string, boost::filesystem::path> clang_location(
      project, "clang.location");
  auto lib_name = absl::StrCat(PLAT_LIB_PFX, lib_base, PLAT_LIB_SFX);
  auto version = clang_version.get();
  if (!version.empty()) {
    auto path = clang_location.get(version);
    if (!path.empty()) {
      auto lib = path / "lib" / lib_name;
      if (boost::filesystem::exists(lib)) return lib;
    }
    for (boost::filesystem::path path :
         {"/lib", "/usr/lib", "/usr/local/lib"}) {
      auto lib = path / absl::StrCat(lib_name, ".", version);
      if (boost::filesystem::exists(lib)) return lib;
    }
  }
  for (boost::filesystem::path path : {"/lib", "/usr/lib", "/usr/local/lib"}) {
    auto lib = path / lib_name;
    if (boost::filesystem::exists(lib)) return lib;
  }
  throw std::runtime_error(
      absl::StrCat("Clang lib '", lib_name, "' not found"));
}

static void ExtractIncludePathsFromTool(const boost::filesystem::path& toolname,
                                        std::vector<std::string>* args) {
  /*
#include <...> search starts here:
 /usr/local/google/home/ctiller/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-14.04/bin/../include/c++/v1
 /usr/local/include
 /usr/local/google/home/ctiller/clang+llvm-4.0.0-x86_64-linux-gnu-ubuntu-14.04/bin/../lib/clang/4.0.0/include
 /usr/include/x86_64-linux-gnu
 /usr/include
End of search list.
  */
  RE2 r_start(R"(#include <...> search starts here:.*)");
  RE2 r_ent(R"(\s+(.*))");
  RE2 r_end(R"(End of search list\..*)");
  bool started = false;
  std::string ent;
  for (auto line : absl::StrSplit(
           run(toolname,
               {"-std=c++11", "-stdlib=libc++", "-v", "-E", "-x", "c++", "-"},
               "")
               .err,
           '\n')) {
    re2::StringPiece spline(line.data(), line.length());
    Log() << "CLANGARGLINE: " << line;
    if (!started && RE2::FullMatch(spline, r_start)) {
      started = true;
    } else if (started && RE2::FullMatch(spline, r_ent, &ent)) {
      args->push_back("-isystem");
      args->push_back(ent);
    } else if (started && RE2::FullMatch(spline, r_end)) {
      started = false;
    }
  }
}

void ClangCompileArgs(Project* project, const boost::filesystem::path& filename,
                      std::vector<std::string>* args) {
  Config<std::string> clang_version(project, "project.clang-version");
  ConfigMap<std::string, boost::filesystem::path> clang_location(
      project, "clang.location");
  if (CompilationDatabase* db = project->aspect<CompilationDatabase>()) {
    if (db->ClangCompileArgs(filename, args)) {
      return;
    }
  }

  args->push_back("-x");
  args->push_back("c++");
  args->push_back("-std=c++11");

#ifdef __APPLE__
  for (auto path : Path()) {
    auto cmd = path / "clang";
    if (boost::filesystem::exists(cmd) &&
        cmd != ClangToolPath(project, "clang")) {
      ExtractIncludePathsFromTool(cmd, args);
    }
  }
#endif

  ExtractIncludePathsFromTool(ClangToolPath(project, "clang"), args);

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

boost::filesystem::path ClangCompileCommand(
    Project* project, const boost::filesystem::path& filename,
    const boost::filesystem::path& src_file,
    const boost::filesystem::path& dst_file, std::vector<std::string>* args) {
  ClangCompileArgs(project, filename, args);

  args->push_back("-g");
  args->push_back("-O2");
  args->push_back("-DNDEBUG");

  args->push_back("-c");
  args->push_back("-o");
  args->push_back(dst_file.string());
  args->push_back(src_file.string());

  return ClangToolPath(project, "clang++");
}
