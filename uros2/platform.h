#pragma once

#include "uros_config.h"

#ifdef PLATFORM_LINUX
#include "../thirdparty/oal/oal_linux.h"
#endif

#ifdef PLATFORM_FREERTOS
#include "../thirdparty/oal/oal_freertos.h"
#endif

#include "../thirdparty/oal/oal_common.h"
