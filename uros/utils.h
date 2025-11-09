#pragma once

#include <cstring>

#include "platform.h"

#include "etl/algorithm.h"
#include "etl/murmur3.h"

namespace uros {

// helper function to get uuid of a specified type
template <typename T> constexpr type_id_t type_id() {
  return reinterpret_cast<const type_id_t>(&type_id<T>);
}

inline int timeout_now(const int &init_timeout_ms,
                       const int64_t &init_stamp_ms) {
  return etl::min(init_timeout_ms,
                  etl::max(init_timeout_ms - int(now_ms() - init_stamp_ms), 0));
}

inline uint32_t calc_entry_hash(const char *entry_name) {
  const uint8_t *begin = reinterpret_cast<const uint8_t *>(entry_name);
  const uint8_t *end = begin + std::strlen(entry_name);
  etl::murmur3<uint32_t> hash(begin, end);
  return hash.value();
}

} // namespace uros
