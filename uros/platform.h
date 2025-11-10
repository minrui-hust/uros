#pragma once

#include "uros_config.h"

#ifdef UROS_VERBOSE
#define UROS_PRINT(...) printf(__VA_ARGS__)
#else
#define UROS_PRINT(...)
#endif

#ifdef PLATFORM_LINUX
#include "oal/oal_linux.h"
#endif

#ifdef PLATFORM_FREERTOS
#include "oal/oal_freertos.h"
#endif

#include "oal/oal_common.h"
