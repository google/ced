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

#include "client.h"
#include <grpc++/create_channel.h>
#include <boost/filesystem.hpp>
#include "project.h"
#include "server.h"
#include "absl/time/clock.h"
#include "log.h"

Client::Client(const boost::filesystem::path& ced_bin, const boost::filesystem::path& path) {
  Project project(path, true);
  auto root = project.aspect<ProjectRoot>();
  auto port_exists = [&](){
    return boost::filesystem::exists(root->LocalAddressPath());
  };
  if (!port_exists()) {
    SpawnServer(ced_bin, project);
    int a = 1, b = 1;
    while (!port_exists()) {
      auto timeout = absl::Milliseconds(a);
      Log() << "Sleeping for " << timeout << " while waiting for server @ " << root->LocalAddress();
      absl::SleepFor(timeout);
      int c = a + b;
      b = a;
      a = c;
    }
  }
  auto channel = grpc::CreateChannel(root->LocalAddress(),
                                     grpc::InsecureChannelCredentials());
  project_stub_ = ProjectService::NewStub(channel);
}
