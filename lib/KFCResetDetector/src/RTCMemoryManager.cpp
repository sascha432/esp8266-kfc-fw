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


bool RTCMemoryManager::_readHeader(RTCMemoryManager::Header_t &header, uint32_t &offset, uint16_t &length) {

    bool result = false;
    offset = __headerOffset;

    if (ESP.rtcUserMemoryRead(offset / __blockSize, (uint32_t *)&header, sizeof(header))) {
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
        uint16_t size = __memorySize - offset;
        memPtr = (uint32_t *)calloc(1, size);
        uint16_t crc = -1;
        if (!ESP.rtcUserMemoryRead(offset / __blockSize, memPtr, size) || ((crc = crc16_calc((const uint8_t *)memPtr, header.length + sizeof(header) - sizeof(header.crc))) != header.crc)) {
            _debug_printf(PSTR("RTC memory: CRC mismatch %04x != %04x, length %d\n"), crc, header.crc, size);
            free(memPtr);
            return nullptr;
        }
    }
    return memPtr;
}

bool RTCMemoryManager::read(uint8_t id, void *dataPtr, uint8_t maxSize)
{
    memset(dataPtr, 0, maxSize);
    _debug_printf_P(PSTR("plugin_read_rtc_memory(%d, %d)\n"), id, maxSize);
    if (id > __maxId) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d): invalid id\n"), id);
        return false;
    }
    uint16_t length;

    // TODO this function could save memory by reading from RTC memory and storing the result directly in dataPtr instead of using __plugin_read_rtc_memory()
    // it has to read the entire data set to generate the CRC though
#if 0
    // not well tested, variable "data" seems to get corrupted
    Header_t header;
    uint32_t offset;
    uint16_t crc = ~0;
    bool result = true;

    if (_readHeader(header, offset, length)) {
        uint8_t *ptr = (uint8_t *)dataPtr;
        uint8_t entryOffset = 0;
        offset /= PLUGIN_RTC_MEM_BLK_SIZE;
        while(length) {
            uint8_t data[4];
            if (!ESP.rtcUserMemoryRead(offset++, (uint32_t *)&data, sizeof(data))) {
                result = false;
                break;
            }
            length -= 4;  // check length?
            bool match = false;
            auto entry = (Entry_t *)(&data[entryOffset]);
            if (!entry->mem_id) {
                break; // done
            }
            else if (entry->mem_id == id) {
                match = true;
            }

            // here it gets a bit complicated
            // it needs to calculate the number of DWORDS to read and the offset for the next entry, since it isn't aligned

            uint16_t bytes = entry->length + sizeof(*entry);
            uint8_t dwords = bytes / 4;
            uint8_t copyOffset = entryOffset + sizeof(*entry);
            if (match && copyOffset < sizeof(data)) {
                memcpy(ptr, data, sizeof(data) - copyOffset);
                ptr += sizeof(data) - copyOffset;
            }

            entryOffset = bytes % 4; // new offset inside the 4 byte data block
            if (!entryOffset) { // no offset for the next DWORD, read it here
                dwords++;
            }
            while(--dwords) { // we got the first DWORD already
                if (!ESP.rtcUserMemoryRead(offset++, (uint32_t *)&data, sizeof(data))) {
                    result = false;
                    break;
                }
                length -= 4; // check length?
                crc = crc16_update(crc, (const uint8_t *)&data, sizeof(data));
                if (match) {
                    memcpy(ptr, &data, sizeof(data));
                    ptr += sizeof(data);
                }
            }
            if (match && entryOffset) { // copy the missing bytes from the beginning of the next DWORD
                if (!ESP.rtcUserMemoryRead(offset, (uint32_t *)ptr, sizeof(data) - entryOffset)) {
                    result = false;
                    break;
                }
                ptr += sizeof(data) - entryOffset;
            }
        }

        // update the CRC with the partial header
        crc = crc16_update(crc, (const uint8_t *)&header, sizeof(header) - sizeof(header.crc));

    }
#endif

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
            free(memPtr);
            return true;
        }
        ptr += entry->length;
    }
    free(memPtr);
    return false;

}

bool RTCMemoryManager::write(uint8_t id, void *dataPtr, uint8_t dataLength) {
    _debug_printf_P(PSTR("plugin_write_rtc_memory(%d, %d)\n"), id, dataLength);

    if (id > __maxId) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d): invalid id\n"), id);
        return false;
    }
    if (dataLength > __maxLength) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d, %d): max size exceeded\n"), id, dataLength);
        return false;
    }

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
        free(memPtr);
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
    *(uint16_t *)(newData.get() + crcPosition) = crc16_calc(newData.get(), header.length + sizeof(header) - sizeof(header.crc));
    auto result = ESP.rtcUserMemoryWrite((__memorySize - newData.length()) / __blockSize, (uint32_t *)newData.get(), newData.length());

    //ESP.rtcMemDump();

    return result;
}

bool RTCMemoryManager::clear() {
    uint8_t blocks = sizeof(Header_t) / __blockSize;
    uint32_t offset = __headerAddress;
    uint32_t data = 0;
    while (blocks--) {
        if (!ESP.rtcUserMemoryWrite(offset++, &data, sizeof(data))) {
            return false;
        }
    }
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
        auto plugin = get_plugin_by_rtc_memory_id(entry->mem_id);
        output.printf_P(PSTR("id: %d (%s), length %d "), entry->mem_id, plugin ? plugin->getPluginName().c_str() : PSTR("<no plugin found>"), entry->length);
#else
        output.printf_P(PSTR("id: %d, length %d "), entry->mem_id, entry->length);
#endif
        dumper.dump(ptr, entry->length);
        ptr += entry->length;
    }

    free(memPtr);
}
#endif
