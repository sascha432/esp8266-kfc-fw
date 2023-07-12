#pragma once
#ifndef __NVS_COMMON_PVT_H_INCUDED
#define __NVS_COMMON_PVT_H_INCUDED 1


#ifndef IDF_DEPRECATED
#ifdef IDF_CI_BUILD
#define IDF_DEPRECATED(REASON) __attribute__((deprecated(REASON)))
#else
#define IDF_DEPRECATED(REASON)
#endif
#endif

#include "../private/esp_err.h"
#include "../private/esp_partition.h"
#include "../private/spi_flash_geometry.h"

#if defined(LINUX_TARGET)
#include <crc.h>
#include "../private//crc.h"
#else
#include "../private/esp_crc.h"
#endif

#include "esp_partition.h"

#undef LINUX_TARGET

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif
