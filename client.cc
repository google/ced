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
#include "src_hash.h"

DEFINE_bool(check_server_version, false,
            "Check the version of the server is the same as the client "
            "version, quit it otherwise");
DEFINE_bool(restart_server, false, "Force the server to restart");

Client::Client(const boost::filesystem::path& ced_bin,
               const boost::filesystem::path& path) {
  Project project(path, true);
  auto root = project.aspect<ProjectRoot>();
  auto port_exists = [&]() {
    return boost::filesystem::exists(root->LocalAddressPath());
  };
  auto unlink_port = [&]() {
    if (!boost::filesystem::remove(root->LocalAddressPath())) {
      throw std::runtime_error(
          absl::StrCat("Failed to remove ", root->LocalAddressPath().string()));
    }
  };

  bool force_restart = FLAGS_restart_server;

  for (int attempt = 1; attempt <= 3; attempt++) {
    if (!port_exists()) {
      force_restart = false;
      SpawnServer(ced_bin, project);
      int a = 1, b = 1;
      while (!port_exists()) {
        auto timeout = absl::Milliseconds(a);
        if (timeout > absl::Seconds(2)) {
          throw std::runtime_error("Failed starting server");
        }
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

    gpr_timespec hello_deadline = gpr_time_add(
        gpr_now(GPR_CLOCK_MONOTONIC), gpr_time_from_seconds(5, GPR_TIMESPAN));

    ConnectionHelloRequest hello_request;
    ConnectionHelloResponse hello_response;
    grpc::ClientContext hello_ctx;
    hello_ctx.set_deadline(hello_deadline);
    auto hello_status = project_stub_->ConnectionHello(
        &hello_ctx, hello_request, &hello_response);
    if (!hello_status.ok()) {
      Log() << "ConnectionHello failed with status: "
            << hello_status.error_code() << " " << hello_status.error_message();
      unlink_port();
      continue;
    }

    if (force_restart || (FLAGS_check_server_version &&
                          hello_response.src_hash() != ced_src_hash)) {
      Log() << "Requesting server to quit";
      Empty req, rsp;
      grpc::ClientContext quit_ctx;
      quit_ctx.set_deadline(hello_deadline);
      project_stub_->Quit(&quit_ctx, req, &rsp);
      unlink_port();
      continue;
    }

    // if we get here, everything looks good to go!
    break;
  }
}

namespace {

class ClientCollaborator : public AsyncCommandCollaborator {
 public:
  ClientCollaborator(const Buffer* buffer, EditStreamPtr stream,
                     std::unique_ptr<grpc::ClientContext> context)
      : AsyncCommandCollaborator("client", absl::Seconds(0), absl::Seconds(0)),
        context_(std::move(context)),
        stream_(std::move(stream)) {}

  void Push(const CommandSet* commands) {
    if (commands == nullptr) {
      Log() << "Cancel context";
      context_->TryCancel();
    } else {
      EditMessage msg;
      *msg.mutable_commands() = *commands;
      stream_->Write(msg);
    }
  }

  bool Pull(CommandSet* commands) {
    commands->Clear();
    EditMessage msg;
    Log() << "Read";
    if (!stream_->Read(&msg)) {
      Log() << "Read failed";
      return false;
    }
    if (msg.type_case() != EditMessage::kCommands) {
      Log() << "Protocol error";
      context_->TryCancel();
      return false;
    }
    *commands = msg.commands();
    return true;
  }

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  EditStreamPtr stream_;
};

}  // namespace

std::unique_ptr<Buffer> Client::MakeBuffer(
    const boost::filesystem::path& path) {
  std::unique_ptr<grpc::ClientContext> ctx(new grpc::ClientContext());
  std::pair<EditStreamPtr, EditMessage> stream_and_first_msg =
      MakeEditStream(ctx.get(), path);
  if (!stream_and_first_msg.first) return nullptr;
  auto buffer =
      Buffer::Builder()
          .SetFilename(path)
          .SetInitialString(AnnotatedString::FromProto(
              stream_and_first_msg.second.server_hello().current_state()))
          .SetSiteID(stream_and_first_msg.second.server_hello().site_id())
          .Make();
  buffer->MakeCollaborator<ClientCollaborator>(
      std::move(stream_and_first_msg.first), std::move(ctx));
  return buffer;
}

std::pair<EditStreamPtr, EditMessage> Client::MakeEditStream(
    grpc::ClientContext* ctx, const boost::filesystem::path& path) {
  EditStreamPtr stream = project_stub_->Edit(ctx);
  EditMessage hello;
  hello.mutable_client_hello()->set_buffer_name(path.string());
  stream->Write(hello);
  if (!stream->Read(&hello)) return std::pair<EditStreamPtr, EditMessage>();
  if (hello.type_case() != EditMessage::kServerHello) {
    return std::pair<EditStreamPtr, EditMessage>();
  }
  return std::make_pair(std::move(stream), hello);
}
