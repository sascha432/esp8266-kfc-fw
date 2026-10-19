/**
 * Author: sascha_lammers@gmx.de
 */

// ESP8266: flash access of the file system without optimistic_yield()
//
// The core's flash_hal_read()/flash_hal_write()/flash_hal_erase() call optimistic_yield(10000) before every
// flash access. LittleFS has no lock on the ESP8266, so a LittleFS call from the loop task that runs longer
// than 10 ms (the LED show alone can take ~30 ms with interrupts off) yielded in the middle of an operation
// and the SDK ran the network callbacks: the async web server read a static file with LittleFS while the
// loop task was writing a log file (logged with KFC_LFS_REENTRY_GUARD, src/lfs_reentry_guard.cpp). The
// shared caches of LittleFS were corrupted and the device crashed in a later commit (Exception 29 in
// memcpy).
//
// The callers are LittleFS.cpp and spiffs_api.cpp of the core, linked with -Wl,--wrap=flash_hal_read etc.
// (conf/common_esp8266.ini). Without the yield every file system call runs to its end, in the loop task as
// well as in the network callbacks, which cannot interrupt each other; the network is stalled for the
// duration of the call, which is intended. Both file systems erase one block (8 KB, two sectors) per call,
// the loop only feeds the software watchdog.
//
// ESP.flashRead()/ESP.flashWrite() are kept: LittleFS passes the caller's buffer through in its cache bypass
// path and the SDK calls need 4 byte aligned addresses and buffers (and PUYA chips a read-modify-write).

#include <global.h>

#if ESP8266

#include <Arduino.h>
#include <flash_hal.h>

extern "C" {

    #include <spi_flash.h>
    #include <user_interface.h>

    int32_t __wrap_flash_hal_read(uint32_t addr, uint32_t size, uint8_t *dst)
    {
        return ESP.flashRead(addr, dst, size) ? FLASH_HAL_OK : FLASH_HAL_READ_ERROR;
    }

    int32_t __wrap_flash_hal_write(uint32_t addr, uint32_t size, const uint8_t *src)
    {
        return ESP.flashWrite(addr, src, size) ? FLASH_HAL_OK : FLASH_HAL_WRITE_ERROR;
    }

    int32_t __wrap_flash_hal_erase(uint32_t addr, uint32_t size)
    {
        if ((size & (SPI_FLASH_SEC_SIZE - 1)) != 0 || (addr & (SPI_FLASH_SEC_SIZE - 1)) != 0) {
            abort();
        }
        const uint32_t sector = addr / SPI_FLASH_SEC_SIZE;
        const uint32_t sectorCount = size / SPI_FLASH_SEC_SIZE;
        for (uint32_t i = 0; i < sectorCount; ++i) {
            system_soft_wdt_feed();
            if (spi_flash_erase_sector(sector + i) != SPI_FLASH_RESULT_OK) {
                return FLASH_HAL_ERASE_ERROR;
            }
        }
        return FLASH_HAL_OK;
    }

}

#endif
