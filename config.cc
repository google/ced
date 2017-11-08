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
#include "log.h"
#include "yaml-cpp/yaml.h"

class ConfigRegistry {
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

  static ConfigRegistry& Get() {
    static ConfigRegistry registry;
    return registry;
  }

 private:
  ConfigRegistry() {
    const char* home = getenv("HOME");
    LoadConfig(absl::StrCat(home, "/.config/ced"));
    LoadConfig(".ced");
    for (auto it = configs_.crbegin(); it != configs_.crend(); ++it) {
      Log() << *it;
    }
  }

  void LoadConfig(const std::string& filename) {
    try {
      configs_.push_back(YAML::LoadFile(filename));
    } catch (std::exception& e) {
      Log() << "Failed opening '" << filename << "': " << e.what();
    }
  }

  void SetWatcher(ConfigWatcher* watcher, const std::string& path)
      EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    auto p = absl::StrSplit(path, '/');
    for (auto it = configs_.crbegin(); it != configs_.crend(); ++it) {
      std::vector<YAML::Node> nstk;
      nstk.push_back(*it);
      for (absl::string_view child : p) {
        std::string child_str(child.data(), child.length());
        nstk.push_back(nstk.back()[child_str]);
      }
      if (nstk.back().IsScalar()) {
        Log() << "CONFIG: " << path << " --> " << nstk.back().Scalar();
        watcher->Set(nstk.back());
        return;
      }
    }
  }

  absl::Mutex mu_;
  std::unordered_map<ConfigWatcher*, std::string> paths_ GUARDED_BY(mu_);
  std::vector<YAML::Node> configs_;
};

void ConfigWatcher::RegisterWatcher(const std::string& path) {
  Log() << "CONFIG.CREATE: " << path;
  ConfigRegistry::Get().RegisterWatcher(this, path);
}

ConfigWatcher::~ConfigWatcher() { ConfigRegistry::Get().RemoveWatcher(this); }
