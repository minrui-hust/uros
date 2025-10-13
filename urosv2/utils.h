#pragma once

#include "platform.h"

namespace uros {

// helper function to get uuid of a specified type
template <typename T> constexpr type_id_t type_id() {
  return reinterpret_cast<const type_id_t>(&type_id<T>);
}

} // namespace uros
