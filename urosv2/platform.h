#pragma once

#include "uros_config.h"

#ifdef UROS_VERBOSE
#define UROS_PRINT(...) printf(...)
#else
#define UROS_PRINT(...)
#endif

#ifdef PLATFORM_LINUX
#include "os_hal/linux.h"
#endif

#ifdef PLATFORM_FREERTOS
#include "os_hal/freertos.h"
#endif

#include "os_hal/common.h"
