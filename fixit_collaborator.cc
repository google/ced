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
#include "fixit_collaborator.h"
#include "absl/strings/str_join.h"

EditResponse FixitCollaborator::Edit(const EditNotification& notification) {
  EditResponse response;
  notification.fixits.ForEach([&](ID fixit_id, const Fixit& fixit) {
    if (fixit.type != Fixit::Type::COMPILE_FIX) return;
    Log() << "CONSUME FIXIT: " << absl::StrJoin(fixit_id, ":");
    notification.fixits.MakeRemove(&response.fixits, fixit_id);
    notification.content.MakeRemove(&response.content, fixit.begin, fixit.end);
    notification.content.MakeInsert(
        &response.content, site(), fixit.replacement,
        String::Iterator(notification.content, fixit.begin).Prev().id());
  });
  return response;
}
