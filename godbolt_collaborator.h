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

#include "buffer.h"

class GodboltCollaborator final : public SyncCollaborator {
 public:
  GodboltCollaborator(const Buffer* buffer)
      : SyncCollaborator("godbolt", absl::Seconds(0)),
        buffer_(buffer),
        side_buffer_editor_(site()),
        side_buffer_ref_editor_(site()) {}

  EditResponse Edit(const EditNotification& notification) override;

 private:
  const Buffer* const buffer_;
  UMapEditor<std::string, SideBuffer> side_buffer_editor_;
  UMapEditor<ID, Annotation<SideBufferRef>> side_buffer_ref_editor_;
  std::string last_compiled_;
};
