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
#include "buffer.h"
#include "proto/project_service.grpc.pb.h"

typedef std::unique_ptr<
    grpc::ClientReaderWriterInterface<EditMessage, EditMessage>>
    EditStreamPtr;

class Client {
 public:
  Client(const boost::filesystem::path& ced_bin,
         const boost::filesystem::path& project_root_hint);

  std::unique_ptr<Buffer> MakeBuffer(const boost::filesystem::path& path);
  std::pair<EditStreamPtr, EditMessage> MakeEditStream(
      grpc::ClientContext* ctx, const boost::filesystem::path& path);

 private:
  std::unique_ptr<ProjectService::Stub> project_stub_;
};
