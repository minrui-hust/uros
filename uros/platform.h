#pragma once

#include "kernel.h"

#include "uros_config.h"

#ifdef UROS_VERBOSE
#define UROS_PRINT(...) printf(...)
#else
#define UROS_PRINT(...)
#endif

#ifdef PLATFORM_LINUX
#include "platform_linux.h"
#endif

#ifdef PLATFORM_FREERTOS
#include "platform_freertos.h"
#endif
