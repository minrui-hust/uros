#pragma once

#include <cstdint>

namespace uros {

// all message should derived from this
struct MsgBase {
  mutable int32_t topic_id; // for internal usage only
};

} // namespace uros
