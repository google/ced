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
#include "config.h"
#include <unordered_map>
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "project.h"
#include "yaml-cpp/yaml.h"

class ConfigRegistry : public ProjectAspect {
 public:
  void RegisterWatcher(ConfigWatcher* watcher, const std::string& path) {
    absl::MutexLock lock(&mu_);
    paths_[watcher] = path;
    SetWatcher(watcher, path);
  }

  void RemoveWatcher(ConfigWatcher* watcher) {
    absl::MutexLock lock(&mu_);
    paths_.erase(watcher);
  }

  ConfigRegistry(Project* project) {
    boost::filesystem::path home = getenv("HOME");
    std::vector<boost::filesystem::path> cfg_paths{
        home / ".config" / "ced" / "config.yaml",
    };
    for (auto c : cfg_paths) {
      LoadConfig(c);
    }
    project->ForEachAspect<ConfigFile>(
        [this](ConfigFile* a) { LoadConfig(a->Config()); });
  }

 private:
  void LoadConfig(const boost::filesystem::path& filename) {
    try {
      configs_.push_back(YAML::LoadFile(filename.string()));
    } catch (std::exception& e) {
    }
  }

  void SetWatcher(ConfigWatcher* watcher, const std::string& path)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    auto p = absl::StrSplit(path, '.');
    for (auto it = configs_.crbegin(); it != configs_.crend(); ++it) {
      std::vector<YAML::Node> nstk;
      nstk.push_back(*it);
      for (absl::string_view child : p) {
        std::string child_str(child.data(), child.length());
        nstk.push_back(nstk.back()[child_str]);
      }
      if (nstk.back()) {
        watcher->Set(nstk.back());
        return;
      }
    }
  }

  absl::Mutex mu_;
  std::unordered_map<ConfigWatcher*, std::string> paths_ GUARDED_BY(mu_);
  std::vector<YAML::Node> configs_;
};

void ConfigWatcher::RegisterWatcher(Project* project, const std::string& path) {
  assert(registry_ == nullptr);
  registry_ = project->aspect<ConfigRegistry>();
  registry_->RegisterWatcher(this, path);
}

ConfigWatcher::~ConfigWatcher() { registry_->RemoveWatcher(this); }

IMPL_PROJECT_GLOBAL_ASPECT(ConfigRegistry, project, 100000) {
  return std::unique_ptr<ProjectAspect>(new ConfigRegistry(project));
}
