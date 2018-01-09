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
#include "server.h"
#include <grpc++/security/server_credentials.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <map>
#include "absl/synchronization/mutex.h"
#include "application.h"
#include "buffer.h"
#include "log.h"
#include "proto/project_service.grpc.pb.h"
#include "run.h"
#include "src_hash.h"

class ProjectServer : public Application, public ProjectService::Service {
 public:
  ProjectServer(int argc, char** argv)
      : project_(PathFromArgs(argc, argv), false),
        active_requests_(0),
        last_activity_(absl::Now()),
        quit_requested_(false) {
    if (PathFromArgs(argc, argv) !=
        project_.aspect<ProjectRoot>()->LocalAddressPath()) {
      throw std::runtime_error(absl::StrCat(
          "Project path misalignment: ", PathFromArgs(argc, argv).string(), " ",
          project_.aspect<ProjectRoot>()->LocalAddressPath().string()));
    }

    server_ =
        grpc::ServerBuilder()
            .RegisterService(this)
            .AddListeningPort(project_.aspect<ProjectRoot>()->LocalAddress(),
                              grpc::InsecureServerCredentials())
            .BuildAndStart();

    Log() << "Created server " << server_.get() << " @ "
          << project_.aspect<ProjectRoot>()->LocalAddress();
  }

  int Run() override {
    auto done = [this]() {
      mu_.AssertHeld();
      return active_requests_ == 0 &&
             (quit_requested_ ||
              (absl::Now() - last_activity_ > absl::Hours(1)));
    };
    {
      absl::MutexLock lock(&mu_);
      while (!mu_.AwaitWithTimeout(absl::Condition(&done),
                                   absl::Duration(absl::Minutes(1))))
        ;
    }
    server_->Shutdown();
    return 0;
  }

  grpc::Status ConnectionHello(grpc::ServerContext* context,
                               const ConnectionHelloRequest* req,
                               ConnectionHelloResponse* rsp) override {
    rsp->set_src_hash(ced_src_hash);
    return grpc::Status::OK;
  }

  grpc::Status Quit(grpc::ServerContext* context, const Empty* req,
                    Empty* rsp) override {
    absl::MutexLock lock(&mu_);
    quit_requested_ = true;
    return grpc::Status::OK;
  }

  grpc::Status Edit(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<EditMessage, EditMessage>* stream) override {
    ScopedRequest scoped_request(this);
    EditMessage msg;
    if (!stream->Read(&msg)) {
      return grpc::Status(grpc::INVALID_ARGUMENT,
                          "Stream closed with no greeting");
    }
    if (msg.type_case() != EditMessage::kClientHello) {
      return grpc::Status(grpc::INVALID_ARGUMENT,
                          "First message from client must be ClientHello");
    }
    Buffer* buffer = GetBuffer(msg.client_hello().buffer_name());
    if (!buffer) {
      return grpc::Status(grpc::INVALID_ARGUMENT,
                          "Unable to access requested buffer");
    }
    Site site;
    auto listener = buffer->Listen(
        [stream, &site](const AnnotatedString& initial) {
          EditMessage out;
          auto body = out.mutable_server_hello();
          body->set_site_id(site.site_id());
          *body->mutable_current_state() = initial.AsProto();
          stream->Write(out);
        },
        [stream](const CommandSet* commands) {
          EditMessage out;
          *out.mutable_commands() = *commands;
          stream->Write(out);
        });
    while (stream->Read(&msg)) {
      if (msg.type_case() != EditMessage::kCommands) {
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            "Expected commands after greetings");
      }
      buffer->PushChanges(&msg.commands(), true);
    }
    CommandSet cleanup_commands;
    buffer->ContentSnapshot().MakeDeleteAttributesBySite(&cleanup_commands,
                                                         site);
    buffer->PushChanges(&msg.commands(), false);
    return grpc::Status::OK;
  }

 private:
  Project project_;
  std::unique_ptr<grpc::Server> server_;

  absl::Mutex mu_;
  int active_requests_ GUARDED_BY(mu_);
  absl::Time last_activity_ GUARDED_BY(mu_);
  std::map<boost::filesystem::path, std::unique_ptr<Buffer>> buffers_
      GUARDED_BY(mu_);
  bool quit_requested_ GUARDED_BY(mu_);

  static bool IsChildOf(boost::filesystem::path needle,
                        boost::filesystem::path haystack) {
    needle = boost::filesystem::absolute(needle);
    haystack = boost::filesystem::absolute(haystack);
    if (haystack.filename() == ".") {
      haystack.remove_filename();
    }
    if (!needle.has_filename()) return false;
    needle.remove_filename();

    std::string needle_str = needle.string();
    std::string haystack_str = haystack.string();
    if (needle_str.length() > haystack_str.length()) return false;

    return std::equal(haystack_str.begin(), haystack_str.end(),
                      needle_str.begin());
  }

  Buffer* GetBuffer(boost::filesystem::path path) {
    path = boost::filesystem::absolute(path);
    if (!IsChildOf(path, project_.aspect<ProjectRoot>()->Path())) {
      Log() << "Attempt to access outside of project sandbox: " << path
            << " in project root " << project_.aspect<ProjectRoot>()->Path();
      return nullptr;
    }
    absl::MutexLock lock(&mu_);
    auto it = buffers_.find(path);
    if (it != buffers_.end()) {
      return it->second.get();
    }
    if (!boost::filesystem::exists(path)) {
      return nullptr;
    }
    return buffers_
        .emplace(
            path,
            Buffer::Builder().SetFilename(path).SetProject(&project_).Make())
        .first->second.get();
  }

  class ScopedRequest {
   public:
    explicit ScopedRequest(ProjectServer* p) : p_(p) {
      absl::MutexLock lock(&p_->mu_);
      p_->active_requests_++;
      p_->last_activity_ = absl::Now();
    }

    ~ScopedRequest() {
      absl::MutexLock lock(&p_->mu_);
      p_->active_requests_--;
      p_->last_activity_ = absl::Now();
    }

   private:
    ProjectServer* const p_;
  };

  static boost::filesystem::path PathFromArgs(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("Expected path");
    return argv[1];
  }
};

REGISTER_APPLICATION(ProjectServer);

void SpawnServer(const boost::filesystem::path& ced_bin,
                 const Project& project) {
  run_daemon(
      ced_bin,
      {
          "-mode",
          "ProjectServer",
          "-logfile",
          (project.aspect<ProjectRoot>()->LocalAddressPath().parent_path() /
           absl::StrCat(".cedlog.server.", ced_src_hash))
              .string(),
          project.aspect<ProjectRoot>()->LocalAddressPath().string(),
      });
}
