/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_RTC_MEMORY_MANAGER
#define DEBUG_RTC_MEMORY_MANAGER 0
#endif

#include <Arduino_compat.h>

#include <push_pack.h>

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

    static const uint8_t __maxId = 32;
    static const uint8_t __maxLength = 64;

private:
    static const uint16_t __memorySize = 512;
    static const uint8_t __blockSize = 4;
    static const uint8_t __blockSizeMask = __blockSize - 1;
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

#if DEBUG
    static void dump(Print &output);
#endif

};

#include <pop_pack.h>
