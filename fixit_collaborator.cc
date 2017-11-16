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
