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
#include "compilation_database.h"
#include "project.h"

class ClangProject final : public ProjectAspect, public CompilationDatabase {
 public:
  ClangProject(const boost::filesystem::path& cc) : compile_commands_(cc) {}

  boost::filesystem::path CompileCommandsFile() const override {
    return compile_commands_;
  }

 private:
  boost::filesystem::path compile_commands_;
};

IMPL_PROJECT_ASPECT(Clang, project, path, -100) {
  auto cc = path / "compile_commands.json";
  if (boost::filesystem::exists(cc)) {
    return std::unique_ptr<ProjectAspect>(new ClangProject(cc));
  }
  return nullptr;
}
