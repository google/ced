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
#include <boost/filesystem.hpp>
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

  void Register(std::function<ProjectAspectPtr(
                    Project* project, const boost::filesystem::path& path)>
                    f,
                int priority) {
    fs_[-priority].push_back(f);
  }

  void Register(std::function<ProjectAspectPtr(Project* project)> f,
                int priority) {
    fs_glob_[-priority].push_back(f);
  }

  void ConstructInto(Project* project, const boost::filesystem::path& path,
                     std::vector<ProjectAspectPtr>* v) {
    for (const auto& pfs : fs_) {
      for (auto fn : pfs.second) {
        auto p = fn(project, path);
        if (p) v->emplace_back(std::move(p));
      }
    }
    for (const auto& pfs : fs_glob_) {
      for (auto fn : pfs.second) {
        auto p = fn(project);
        if (p) v->emplace_back(std::move(p));
      }
    }
  }

 private:
  std::map<int, std::vector<std::function<ProjectAspectPtr(
                    Project*, const boost::filesystem::path& path)>>>
      fs_;
  std::map<int, std::vector<std::function<ProjectAspectPtr(Project*)>>>
      fs_glob_;
};

class SafeBackupAspect final : public ProjectAspect, public ProjectRoot {
 public:
  SafeBackupAspect(const boost::filesystem::path& cwd) : cwd_(cwd) {}

  boost::filesystem::path Path() const override { return cwd_; }

 private:
  boost::filesystem::path cwd_;
};

}  // namespace

void Project::RegisterAspectFactory(
    std::function<ProjectAspectPtr(Project* project,
                                   const boost::filesystem::path&)>
        f,
    int priority) {
  AspectRegistry::Get().Register(f, priority);
}

void Project::RegisterGlobalAspectFactory(
    std::function<ProjectAspectPtr(Project* project)> f, int priority) {
  AspectRegistry::Get().Register(f, priority);
}

Project::Project(const boost::filesystem::path& root_hint, bool client_peek)
    : client_peek_(client_peek) {
  boost::filesystem::path path = boost::filesystem::absolute(root_hint);
  if (!boost::filesystem::is_directory(path)) {
    path = path.parent_path();
  }
  boost::filesystem::path bottom = path;
  while (!path.empty()) {
    AspectRegistry::Get().ConstructInto(this, path, &aspects_);
    path = path.parent_path();
  }
  aspects_.emplace_back(new SafeBackupAspect(bottom));
}
