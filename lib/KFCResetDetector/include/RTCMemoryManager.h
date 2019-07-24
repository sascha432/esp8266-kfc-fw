/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_RTC_MEMORY_MANAGER
#define DEBUG_RTC_MEMORY_MANAGER 0
#endif

#include <Arduino_compat.h>

#include <push_pack.h>

// any data stored in RTC memory is kept during deep sleep and device reboots
// data integrity is ensured
// NOTE for ESP32: during normal boots, the data is erased and the reset button seems to be detected as normal boot (tested with heltec wifi lora32)

class RTCMemoryManager {
public:
    typedef struct __attribute__packed__ {
        uint16_t length;
        uint16_t crc;
    } Header_t;

    typedef struct __attribute__packed__ {
        uint8_t mem_id;
        uint8_t length;
    } Entry_t;

public:
#if defined(ESP8266)
    static const uint16_t __memorySize = 512; // fixed
    static const uint8_t __blockSize = 4;
#elif defined(ESP32)
    static const uint16_t __memorySize = 256; // can be adjusted
    static const uint8_t __blockSize = 1;
#endif
    static const uint8_t __blockSizeMask = __blockSize - 1;
    // keep header dword aligned
    static const uint16_t __dataBlockSize = (sizeof(Header_t) & __blockSizeMask ? ((sizeof(Header_t) + 4) & ~__blockSizeMask) : (sizeof(Header_t))) / __blockSize;
    static const uint16_t __dataSize = __dataBlockSize * __blockSize;
    static const uint16_t __headerOffset = __memorySize - __dataSize;
    static const uint16_t __headerAddress = __headerOffset / __blockSize;

    static constexpr bool __isHeaderAligned() {
        return (sizeof(Header_t) & __blockSizeMask) == 0;
    }

public:
    static bool _readHeader(Header_t &header, uint32_t &offset, uint16_t &length);
    static uint32_t *_readMemory(uint16_t &length);

    static bool read(uint8_t id, void *, uint8_t maxSize);
    static bool write(uint8_t id, void *, uint8_t maxSize);
    static bool clear();

    static inline void freeMemPtr(uint32_t *memPtr) {
#if !defined(ESP32)
        free(memPtr);
#endif
    }

#if DEBUG
    static void dump(Print &output);
#endif

};

#include <pop_pack.h>
