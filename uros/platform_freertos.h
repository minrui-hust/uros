#pragma once

#include <cstdint>

#include "kernel.h"
#include "lib/cxx_wrapper/cxx_wrapper.h"

#define UROS_ASSERT(expr) assert(expr)

#ifdef UROS_VERBOSE
#define UROS_PRINT (void)
#else
#define UROS_PRINT (void)
#endif

namespace uros {

using type_id_t = uint32_t;
using EventBits = EventBits_t;

} // namespace uros
