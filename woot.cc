#include "woot.h"

std::atomic<uint64_t> Site::id_gen_;
Site String::root_site_;
ID String::begin_id_ = root_site_.GenerateID();
ID String::end_id_ = root_site_.GenerateID();
