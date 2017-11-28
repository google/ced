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
#include "absl/synchronization/mutex.h"
#include "application.h"
#include "log.h"
#include "proto/project_service.grpc.pb.h"
#include "run.h"

class ProjectServer : public Application, public ProjectService::Service {
 public:
  ProjectServer(int argc, char** argv)
      : project_(PathFromArgs(argc, argv), false) {
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

  int Run() {
    while (!Done()) {
      absl::SleepFor(absl::Seconds(10));
    }
    server_->Shutdown();
    return 0;
  }

 private:
  Project project_;
  std::unique_ptr<grpc::Server> server_;

  absl::Mutex mu_;
  int active_requests_ GUARDED_BY(mu_);
  absl::Time last_activity_ GUARDED_BY(mu_);

  bool Done() {
    absl::MutexLock lock(&mu_);
    return active_requests_ == 0 &&
           absl::Now() - last_activity_ > absl::Hours(1);
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
  run_daemon(ced_bin,
             {
                 "-mode",
                 "ProjectServer",
                 project.aspect<ProjectRoot>()->LocalAddressPath().string(),
             });
}
