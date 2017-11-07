#include "clang_config.h"
#include <stdexcept>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "config.h"

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

std::string ClangCompileCommand(const std::string& filename,
                                const std::string& src_file,
                                const std::string& dst_file) {
  return absl::StrCat(ClangToolPath("clang++"),
                      " -x c++ -std=c++11 -g -O2 -c -o ", dst_file, " ",
                      src_file);
}
