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

#include <boost/filesystem/path.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "absl/strings/str_cat.h"
#include "src_hash.h"

class ProjectAspect {
 public:
  virtual ~ProjectAspect() {}
};
typedef std::unique_ptr<ProjectAspect> ProjectAspectPtr;

class ProjectRoot {
 public:
  virtual boost::filesystem::path Path() const = 0;

  boost::filesystem::path LocalAddressPath() const {
    return (Path() / ".cedport." / ced_src_hash).string();
  }

  std::string LocalAddress() const {
    return absl::StrCat("unix:", LocalAddressPath().string());
  }
};

class ConfigFile {
 public:
  virtual boost::filesystem::path Config() const = 0;
};

class Project {
 public:
  Project(const boost::filesystem::path& path_hint);

  static void RegisterAspectFactory(
      std::function<ProjectAspectPtr(Project* project,
                                     const boost::filesystem::path& path)>,
      int priority);
  static void RegisterGlobalAspectFactory(
      std::function<ProjectAspectPtr(Project* project)>, int priority);

  template <class T>
  T* aspect() {
    for (const auto& pa : aspects_) {
      T* pt = dynamic_cast<T*>(pa.get());
      if (pt != nullptr) return pt;
    }
    return nullptr;
  }

  template <class T>
  void ForEachAspect(std::function<void(T*)> f) {
    for (const auto& pa : aspects_) {
      T* pt = dynamic_cast<T*>(pa.get());
      if (pt != nullptr) f(pt);
    }
  }

 private:
  std::vector<ProjectAspectPtr> aspects_;
};

#define IMPL_PROJECT_ASPECT(name, proj_arg, path_arg, priority)               \
  namespace {                                                                 \
  class name##_impl {                                                         \
   public:                                                                    \
    static std::unique_ptr<ProjectAspect> Construct(                          \
        Project* proj_arg, const boost::filesystem::path& path_arg);          \
    name##_impl() { Project::RegisterAspectFactory(&Construct, (priority)); } \
  };                                                                          \
  name##_impl impl;                                                           \
  }                                                                           \
  std::unique_ptr<ProjectAspect> name##_impl::Construct(                      \
      Project* proj_arg, const boost::filesystem::path& path_arg)

#define IMPL_PROJECT_GLOBAL_ASPECT(name, proj_arg, priority)            \
  namespace {                                                           \
  class name##_impl {                                                   \
   public:                                                              \
    static std::unique_ptr<ProjectAspect> Construct(Project* proj_arg); \
    name##_impl() {                                                     \
      Project::RegisterGlobalAspectFactory(&Construct, (priority));     \
    }                                                                   \
  };                                                                    \
  name##_impl impl;                                                     \
  }                                                                     \
  std::unique_ptr<ProjectAspect> name##_impl::Construct(Project* proj_arg)
