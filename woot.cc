#include "woot.h"

namespace woot {

std::atomic<uint64_t> Site::id_gen_;
Site StringBase::root_site_;
ID StringBase::begin_id_ = root_site_.GenerateID();
ID StringBase::end_id_ = root_site_.GenerateID();

}  // namespace woot
