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

#define ESP_PLATFORM 1
#define DEBUG_NVS_FLASH 0

#include "../private/esp_err.h"
#include "../private/esp_partition.h"
#include "../private/spi_flash_geometry.h"

#include <coredecls.h>

#ifdef __cplusplus
extern "C" {
#endif

inline uint32_t crc32_le(uint32_t crc, uint8_t const *buf, uint32_t len)
{
    return crc32(buf, len, crc);
}

#ifdef __cplusplus
}
#endif

#endif
