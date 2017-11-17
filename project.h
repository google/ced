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
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class ProjectAspect {
 public:
  virtual ~ProjectAspect() {}
};
typedef std::unique_ptr<ProjectAspect> ProjectAspectPtr;

class ProjectRoot {
 public:
  virtual std::string Path() const = 0;
};

class ConfigFile {
 public:
  virtual std::string Config() const = 0;
};

class Project {
 public:
  static Project& Get() {
    static Project project;
    return project;
  }

  static void RegisterAspectFactory(
      std::function<ProjectAspectPtr(const std::string& path)>, int priority);

  template <class T>
  T* aspect() {
    for (const auto& pa : aspects_) {
      T* pt = dynamic_cast<T*>(pa.get());
      if (pt != nullptr) return pt;
    }
    return nullptr;
  }

  template <class T>
  static T* GetAspect() {
    return Get().aspect<T>();
  }

  template <class T>
  static void ForEachAspect(std::function<void(T*)> f) {
    for (const auto& pa : Get().aspects_) {
      T* pt = dynamic_cast<T*>(pa.get());
      if (pt != nullptr) f(pt);
    }
  }

 private:
  Project();

  std::vector<ProjectAspectPtr> aspects_;
};

#define IMPL_PROJECT_ASPECT(name, path_arg, priority)                         \
  namespace {                                                                 \
  class name##_impl {                                                         \
   public:                                                                    \
    static std::unique_ptr<ProjectAspect> Construct(                          \
        const std::string& path_arg);                                         \
    name##_impl() { Project::RegisterAspectFactory(&Construct, (priority)); } \
  };                                                                          \
  name##_impl impl;                                                           \
  }                                                                           \
  std::unique_ptr<ProjectAspect> name##_impl::Construct(                      \
      const std::string& path_arg)
