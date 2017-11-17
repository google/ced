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
#include "compilation_database.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "read.h"
#include "src/json.hpp"

static bool Exists(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  fclose(f);
  return true;
}

struct CompilationDatabase::Impl {
  absl::Mutex mu;
  nlohmann::json js GUARDED_BY(mu);
  std::map<std::string, std::unique_ptr<std::vector<std::string>>> cache
      GUARDED_BY(mu);
};

CompilationDatabase::CompilationDatabase() : impl_(new Impl) {}

CompilationDatabase::~CompilationDatabase() {}

bool CompilationDatabase::ClangCompileArgs(const std::string& filename,
                                           std::vector<std::string>* args) {
  absl::MutexLock lock(&impl_->mu);
  auto it = impl_->cache.find(filename);
  if (it != impl_->cache.end()) {
    if (!it->second) return false;
    *args = *it->second;
    return true;
  }

  if (impl_->js.is_null()) {
    impl_->js = nlohmann::json::parse(Read(CompileCommandsFile()));
  }
  for (const auto& child : impl_->js) {
    if (child["file"] == filename) {
      args->clear();
      std::string command = child["command"];
      std::string directory = child["directory"];
      bool skip_arg = true;
      for (auto arg_view : absl::StrSplit(command, " ")) {
        if (skip_arg) {
          skip_arg = false;
          continue;
        }
        if (arg_view == "-c" || arg_view == "-o") {
          skip_arg = true;
          continue;
        }
        std::string arg(arg_view.data(), arg_view.length());
        if (arg.length() && arg[0] != '/' && arg[0] != '-') {
          auto as_if_under_dir = absl::StrCat(directory, "/", arg);
          if (Exists(as_if_under_dir)) {
            arg = as_if_under_dir;
          }
        }
        args->emplace_back(std::move(arg));
      }
      impl_->cache.emplace(
          std::make_pair(filename, std::unique_ptr<std::vector<std::string>>(
                                       new std::vector<std::string>(*args))));
      return true;
    }
  }
  // cache negative result
  impl_->cache[filename];
  return false;
}
