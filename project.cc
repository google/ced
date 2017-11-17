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
#include "project.h"
#include <sys/param.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace {

class AspectRegistry {
 public:
  static AspectRegistry& Get() {
    static AspectRegistry registry;
    return registry;
  }

  void Register(std::function<ProjectAspectPtr(const std::string& path)> f,
                int priority) {
    fs_[-priority].push_back(f);
  }

  void ConstructInto(const std::string& path,
                     std::vector<ProjectAspectPtr>* v) {
    for (const auto& pfs : fs_) {
      for (auto fn : pfs.second) {
        auto p = fn(path);
        if (p) v->emplace_back(std::move(p));
      }
    }
  }

 private:
  std::map<int, std::vector<
                    std::function<ProjectAspectPtr(const std::string& path)>>>
      fs_;
};

class SafeBackupAspect final : public ProjectAspect, public ProjectRoot {
 public:
  SafeBackupAspect(const std::string& cwd) : cwd_(cwd) {}

  std::string Path() const override { return cwd_; }

 private:
  std::string cwd_;
};

}  // namespace

void Project::RegisterAspectFactory(
    std::function<ProjectAspectPtr(const std::string&)> f, int priority) {
  AspectRegistry::Get().Register(f, priority);
}

Project::Project() {
  // some path above this one is the root of the project... let's try to figure
  // that out based on whatever hints we might see
  char cwd[MAXPATHLEN];
  if (nullptr == getcwd(cwd, sizeof(cwd))) {
    throw std::runtime_error("Couldn't get cwd");
  }
  std::vector<absl::string_view> segments = absl::StrSplit(cwd, '/');
  while (!segments.empty()) {
    auto path = absl::StrJoin(segments, "/");
    AspectRegistry::Get().ConstructInto(path, &aspects_);
    segments.pop_back();
  }
  aspects_.emplace_back(new SafeBackupAspect(cwd));
}
