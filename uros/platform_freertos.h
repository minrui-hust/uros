#pragma once

#include "kernel.h"

#define UROS_ASSERT(expr) assert(expr)

#ifdef UROS_VERBOSE
#define UROS_PRINT (void)
#else
#define UROS_PRINT (void)
#endif
