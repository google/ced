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

#include "application.h"
#include "client.h"

class PeepShow : public Application {
 public:
  PeepShow(int argc, char** argv)
      : client_(argv[0], FileFromCmdLine(argc, argv)) {
    auto stream_and_first_msg =
        client_.MakeEditStream(&ctx_, FileFromCmdLine(argc, argv));
    Log() << stream_and_first_msg.second.DebugString();
    stream_ = std::move(stream_and_first_msg.first);
  }

  ~PeepShow() {}

  int Run() {
    EditMessage msg;
    while (stream_->Read(&msg)) {
      Log() << msg.DebugString();
    }
    return 0;
  }

 private:
  Client client_;
  grpc::ClientContext ctx_;
  EditStreamPtr stream_;

  static boost::filesystem::path FileFromCmdLine(int argc, char** argv) {
    if (argc != 2) {
      throw std::runtime_error("Expected filename to open");
    }
    return argv[1];
  }
};

REGISTER_APPLICATION(PeepShow);
