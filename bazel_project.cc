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
#include <boost/filesystem.hpp>
#include "absl/strings/str_cat.h"
#include "project.h"

class BazelProject final : public ProjectAspect, public ProjectRoot {
 public:
  BazelProject(const boost::filesystem::path& root) : root_(root) {}

  boost::filesystem::path Path() const override { return root_; }

 private:
  boost::filesystem::path root_;
};

IMPL_PROJECT_ASPECT(Bazel, project, path, 0) {
  if (boost::filesystem::exists(path / "WORKSPACE")) {
    return std::unique_ptr<ProjectAspect>(new BazelProject(path));
  }
  return nullptr;
}
