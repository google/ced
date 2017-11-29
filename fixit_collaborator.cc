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
#include "absl/strings/str_join.h"
#include "buffer.h"
#include "log.h"

class FixitCollaborator final : public SyncCollaborator {
 public:
  FixitCollaborator(const Buffer* buffer)
      : SyncCollaborator("fixit", absl::Milliseconds(1500),
                         absl::Milliseconds(100)),
        buffer_(buffer) {}

  EditResponse Edit(const EditNotification& notification) override;

 private:
  const Buffer* const buffer_;
};

EditResponse FixitCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  notification.content.ForEachAnnotation(
      Attribute::kFixit, [&](ID annid, ID beg, ID end, const Attribute& attr) {
        Log() << "FIXIT: " << attr.DebugString();
        const Fixit& fixit = attr.fixit();
        if (fixit.type() != Fixit::COMPILE_FIX) return;
        Log() << "CONSUME FIXIT: " << annid.id;
        AnnotatedString::MakeDelMark(&response.content_updates, annid);
        notification.content.MakeDelete(&response.content_updates, beg, end);
        notification.content.MakeInsert(
            &response.content_updates, buffer_->site(), fixit.replacement(),
            AnnotatedString::Iterator(notification.content, beg).Prev().id());
      });
  return response;
}

SERVER_COLLABORATOR(FixitCollaborator, buffer) { return !buffer->read_only(); }
