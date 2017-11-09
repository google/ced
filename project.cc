#include "project.h"
#include <sys/param.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace {
struct InterestingFile {
  std::string name;
  bool is_file;
  int project_score;
};

struct WhatsThere {
  bool bazel_workspace;
  bool bazel_build;
  bool git;
  bool ced;
  bool gitignore;
  bool clang_format;
  bool license;
  bool readme;
  bool code_of_conduct;
  bool makefile;
  bool github;
  bool vscode;
  bool clang_complete;
  bool travis;
};
}

Project::Project() {
  // some path above this one is the root of the project... let's try to figure
  // that out based on whatever hints we might see
  char cwd[MAXPATHLEN];
  if (nullptr == getcwd(cwd, sizeof(cwd))) {
    throw std::runtime_error("Couldn't get cwd");
  }
  std::cerr << cwd << "\n";
  std::vector<absl::string_view> segments = absl::StrSplit(cwd, '/');
  while (!segments.empty()) {
    auto path = absl::StrJoin(segments, "/");
    std::cerr << path << "\n";
    segments.pop_back();
  }
}
