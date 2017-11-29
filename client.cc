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
#include "absl/time/clock.h"
#include "log.h"
#include "project.h"
#include "server.h"

Client::Client(const boost::filesystem::path& ced_bin,
               const boost::filesystem::path& path) {
  Project project(path, true);
  auto root = project.aspect<ProjectRoot>();
  auto port_exists = [&]() {
    return boost::filesystem::exists(root->LocalAddressPath());
  };
  if (!port_exists()) {
    SpawnServer(ced_bin, project);
    int a = 1, b = 1;
    while (!port_exists()) {
      auto timeout = absl::Milliseconds(a);
      Log() << "Sleeping for " << timeout << " while waiting for server @ "
            << root->LocalAddress();
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

namespace {

typedef std::unique_ptr<
    grpc::ClientReaderWriterInterface<EditMessage, EditMessage>>
    EditStreamPtr;

class ClientCollaborator : public AsyncCommandCollaborator {
 public:
  ClientCollaborator(const Buffer* buffer, EditStreamPtr stream,
                     std::unique_ptr<grpc::ClientContext> context)
      : AsyncCommandCollaborator("client", absl::Seconds(0), absl::Seconds(0)),
        context_(std::move(context)),
        stream_(std::move(stream)) {}

  void Push(const CommandSet* commands) {
    if (commands == nullptr) {
      context_->TryCancel();
    } else {
      EditMessage msg;
      *msg.mutable_commands() = *commands;
      Log() << "CLIENT_WRITE: " << msg.DebugString();
      stream_->Write(msg);
    }
  }

  void Pull(CommandSet* commands) {
    commands->Clear();
    EditMessage msg;
    if (!stream_->Read(&msg)) return;
    if (msg.type_case() != EditMessage::kCommands) return;
    Log() << "CLIENT_READ: " << msg.DebugString();
    *commands = msg.commands();
  }

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  EditStreamPtr stream_;
};

}  // namespace

std::unique_ptr<Buffer> Client::MakeBuffer(
    const boost::filesystem::path& path) {
  std::unique_ptr<grpc::ClientContext> ctx(new grpc::ClientContext());
  EditStreamPtr stream = project_stub_->Edit(ctx.get());
  EditMessage hello;
  hello.mutable_client_hello()->set_buffer_name(path.string());
  stream->Write(hello);
  if (!stream->Read(&hello)) return nullptr;
  if (hello.type_case() != EditMessage::kServerHello) return nullptr;
  auto buffer = Buffer::Builder()
                    .SetFilename(path)
                    .SetInitialString(AnnotatedString::FromProto(
                        hello.server_hello().current_state()))
                    .SetSiteID(hello.server_hello().site_id())
                    .Make();
  buffer->MakeCollaborator<ClientCollaborator>(std::move(stream),
                                               std::move(ctx));
  return buffer;
}
