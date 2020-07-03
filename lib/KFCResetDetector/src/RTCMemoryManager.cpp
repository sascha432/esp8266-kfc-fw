/**
* Author: sascha_lammers@gmx.de
*/

#include "RTCMemoryManager.h"
#include <crc16.h>
#include <Buffer.h>

#if DEBUG_RTC_MEMORY_MANAGER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#if defined(ESP8266)

#define system_rtc_mem_read                     ESP.rtcUserMemoryRead
#define system_rtc_mem_write                    ESP.rtcUserMemoryWrite

#elif defined(ESP32)

RTC_NOINIT_ATTR uint8_t RTCMemoryManager_allocated_block[RTCMemoryManager::__memorySize];

bool system_rtc_mem_read(size_t ofs, uint32_t *data, size_t len) {
    memcpy(data, RTCMemoryManager_allocated_block + ofs * RTCMemoryManager::__blockSize, len);
    return true;
}

bool system_rtc_mem_write(size_t ofs, uint32_t *data, size_t len) {
    memcpy(RTCMemoryManager_allocated_block + ofs * RTCMemoryManager::__blockSize, data, len);
    return true;
}

#endif

bool RTCMemoryManager::_readHeader(RTCMemoryManager::Header_t &header, uint32_t &offset, uint16_t &length) {

    bool result = false;
    offset = __headerOffset;

    if (system_rtc_mem_read(offset / __blockSize, (uint32_t *)&header, sizeof(header))) {
        if (header.length == 0) {
            _debug_printf_P(PSTR("RTC memory: Empty\n"));
        } else if (header.length & __blockSizeMask) {
            _debug_printf_P(PSTR("RTC memory: Stored length not DWORD aligned: %d\n"), header.length);
        } else {
            offset = __memorySize - __dataSize - header.length;
            if (offset > __headerOffset) {
                _debug_printf_P(PSTR("RTC memory: Invalid length, start offset %d\n"), offset);
            } else {
                length = header.length;
                result = true;
            }
        }
    }
    if (!result) {
        debug_printf_P(PSTR("Failed to read RTC memory\n"));
    }
    return result;
}

uint32_t *RTCMemoryManager::_readMemory(uint16_t &length) {

    uint32_t *memPtr = nullptr;
    Header_t header;
    uint32_t offset;

    if (_readHeader(header, offset, length)) {
#if defined(ESP32)
        // we can use the RTC memory directly
        memPtr = reinterpret_cast<uint32_t *>(RTCMemoryManager_allocated_block + offset);
        uint16_t crc = crc16_calc((const uint8_t *)memPtr, header.length + sizeof(header) - sizeof(header.crc));
        if (crc != header.crc) {
            _debug_printf(PSTR("RTC memory: CRC mismatch %04x != %04x, length %d\n"), crc, header.crc, header.length);
            return nullptr;
        }
#else
        uint16_t size = __memorySize - offset;
        memPtr = (uint32_t *)calloc(1, size);
        uint16_t crc = -1;
        if (!system_rtc_mem_read(offset / __blockSize, memPtr, size) || ((crc = crc16_update(memPtr, header.length + sizeof(header) - sizeof(header.crc))) != header.crc)) {
            _debug_printf(PSTR("RTC memory: CRC mismatch %04x != %04x, length %d\n"), crc, header.crc, size);
            freeMemPtr(memPtr);
            return nullptr;
        }
#endif
    }
    return memPtr;
}

bool RTCMemoryManager::read(uint8_t id, void *dataPtr, uint8_t maxSize)
{
    memset(dataPtr, 0, maxSize);
    _debug_printf_P(PSTR("plugin_read_rtc_memory(%d, %d)\n"), id, maxSize);
    uint16_t length;

    auto memPtr = _readMemory(length);
    if (!memPtr) {
        return false;
    }

    auto ptr = (uint8_t *)memPtr;
    auto endPtr = ((uint8_t *)memPtr) + length;
    while(ptr < endPtr) {
        auto entry = (Entry_t *)ptr;
        ptr += sizeof(*entry);
        if (ptr + entry->length > endPtr) {
            _debug_printf_P(PSTR("RTC memory: entry length exceeds total size. id %d, length %d\n"), entry->mem_id, entry->length);
            break;
        }
        if (entry->mem_id == id) {
            if (entry->length > maxSize) {
                entry->length = maxSize;
            }
            memcpy(dataPtr, ptr, entry->length);
            freeMemPtr(memPtr);
            return true;
        }
        ptr += entry->length;
    }
    freeMemPtr(memPtr);
    return false;

}

bool RTCMemoryManager::write(uint8_t id, void *dataPtr, uint8_t dataLength) {
    _debug_printf_P(PSTR("plugin_write_rtc_memory(%d, %d)\n"), id, dataLength);

    Buffer newData;
    uint16_t length;
    auto memPtr = _readMemory(length);
    if (memPtr) {
        // copy existing data
        auto ptr = (uint8_t *)memPtr;
        auto endPtr = ((uint8_t *)memPtr) + length;
        while(ptr < endPtr) {
            auto entry = (Entry_t *)ptr;
            if (!entry->mem_id) { // invalid id, NUL bytes for alignment
                break;
            }
            ptr += sizeof(*entry);
            if (entry->mem_id != id) {
                newData.write((const uint8_t *)entry, sizeof(*entry) + entry->length);
            }
            ptr += entry->length;
        }
        freeMemPtr(memPtr);
    }

    // append new data
    Entry_t newEntry;
    newData.reserve(newData.length() + sizeof(newEntry) + dataLength);
    newEntry.mem_id = id;
    newEntry.length = dataLength;

    newData.write((const uint8_t *)&newEntry, sizeof(newEntry));
    newData.write((const uint8_t *)dataPtr, dataLength);

    // align before adding header
    Header_t header;
    header.length = (uint16_t)newData.length();
    if (header.length & __blockSizeMask) {
        uint8_t alignment = __blockSize - (header.length % __blockSize);
        while (alignment--) {
            header.length++;
            newData.write(0);
        }
    }

    // append header and align
    uint16_t crcPosition = (uint16_t)(newData.length() + sizeof(header) - sizeof(header.crc));
    newData.write((const uint8_t *)&header, sizeof(header));

    if (!__isHeaderAligned()) {
        uint8_t alignment = __blockSize - (sizeof(Header_t) & __blockSizeMask);
        while (alignment--) {
            newData.write(0);
        }
    }

    // update CRC in newData and store
    *(uint16_t *)(newData.get() + crcPosition) = crc16_update(newData.get(), header.length + sizeof(header) - sizeof(header.crc));
    auto result = system_rtc_mem_write((__memorySize - newData.length()) / __blockSize, (uint32_t *)newData.get(), newData.length());

    //ESP.rtcMemDump();

    return result;
}

bool RTCMemoryManager::clear() {
#if defined(ESP8266)

    // clear header only
    uint8_t blocks = sizeof(Header_t) / __blockSize;
    uint32_t offset = __headerAddress;
    uint32_t data = 0;
    while (blocks--) {
        if (!system_rtc_mem_write(offset++, &data, sizeof(data))) {
            return false;
        }
    }

#elif defined(ESP32)

    // clear entire block
    memset(RTCMemoryManager_allocated_block, 0, sizeof(RTCMemoryManager_allocated_block));

#endif
    return true;
}

#if DEBUG

#include <DumpBinary.h>
#include "plugins.h"

void RTCMemoryManager::dump(Print &output) {

    uint16_t length;
    auto memPtr = _readMemory(length);
    if (!memPtr) {
        _debug_printf_P(PSTR("plugin_debug_dump_rtc_memory(): read returned nullptr\n"));
        return;
    }

    output.printf_P(PSTR("RTC data length: %u\n"), length);

    DumpBinary dumper(output);
    auto ptr = (uint8_t *)memPtr;
    auto endPtr = ((uint8_t *)memPtr) + length;
    while(ptr < endPtr) {
        auto entry = (Entry_t *)ptr;
        if (!entry->mem_id) {
            break;
        }
        ptr += sizeof(*entry);
        if (ptr + entry->length > endPtr) {
            _debug_printf_P(PSTR("RTC memory: entry length exceeds total size. id %d, block count %d\n"), entry->mem_id, entry->length);
            break;
        }
#if HAVE_KFC_PLUGINS
        auto plugin = PluginComponent::getByMemoryId(entry->mem_id);
        output.printf_P(PSTR("id: %d (%s), length %d "), entry->mem_id, plugin ? plugin->getName() : PSTR("<no plugin found>"), entry->length);
#else
        output.printf_P(PSTR("id: %d, length %d "), entry->mem_id, entry->length);
#endif
        dumper.dump(ptr, entry->length);
        ptr += entry->length;
    }

    freeMemPtr(memPtr);
}
#endif
