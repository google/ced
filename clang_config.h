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
#include <string>
#include <vector>

#include "project.h"

boost::filesystem::path ClangToolPath(Project* project,
                                      const std::string& name);
boost::filesystem::path ClangLibPath(Project* project, const std::string& name);
boost::filesystem::path ClangCompileCommand(
    Project* project, const boost::filesystem::path& filename,
    const boost::filesystem::path& src_file,
    const boost::filesystem::path& dst_file, std::vector<std::string>* args);
void ClangCompileArgs(Project* project, const boost::filesystem::path& filename,
                      std::vector<std::string>* args);
