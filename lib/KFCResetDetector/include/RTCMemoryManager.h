/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_RTC_MEMORY_MANAGER
#define DEBUG_RTC_MEMORY_MANAGER                    0
#endif

#include <Arduino_compat.h>
#include <push_pack.h>

#if HAVE_KFC_FIRMWARE_VERSION
#include <PluginComponent.h>
#endif

// any data stored in RTC memory is kept during deep sleep and device reboots
// data integrity is ensured
// NOTE for ESP32: during normal boots, the data is erased and the reset button seems to be detected as normal boot (tested with heltec wifi lora32)

class RTCMemoryManager {
public:
#if HAVE_KFC_FIRMWARE_VERSION
    using RTCMemoryId = PluginComponent::RTCMemoryId;
#else
    using RTCMemoryId = uint8_t;
#endif

#if defined(ESP8266)
    static constexpr uint16_t kMemorySize = 64 * 4; // fixed
    // static constexpr uint16_t kMemoryLimit = kMemorySize - sizeof(Header_t);
    static constexpr uint8_t kBlockSize = 4;
    static constexpr uint8_t kBaseAddress = 64;
    static constexpr uint8_t kLastAddress = kBaseAddress + (kMemorySize / kBlockSize) - 1;

    static constexpr uint8_t kTimeMemorySlot = kBaseAddress + 2;

#elif defined(ESP32)
    static const uint16_t kMemorySize = 256; // can be adjusted
    static const uint8_t kBlockSize = 1;
    static constexpr uint8_t kBaseOffset = 0;
#endif

    struct Header_t {
        uint16_t length;
        uint16_t crc;
        inline uint16_t data_length() const {
            return length - sizeof(Header_t);
        }
        inline uint16_t crc_offset() const {
            return data_length() + offsetof(Header_t, crc);
        }
        inline uint16_t crc_length() const {
            return crc_offset();
        }
        inline uint16_t start_offset() const {
            return kMemorySize - length;
        }
        inline uint16_t start_address() const {
            return (kMemorySize - length) / kBlockSize + kBaseAddress;
        }
        inline uint8_t *begin(void *ptr) const {
            return reinterpret_cast<uint8_t *>(ptr);
        }
        inline uint8_t *end(void *ptr) const {
            return begin(ptr) + data_length();
        }
        inline size_t distance(void *stast, void *iterator) {
            return end(iterator) - begin(stast);
        }
    };

    struct Entry_t {
        uint8_t mem_id;
        uint8_t length;

        Entry_t() : mem_id(0), length(0) {}
        Entry_t(const uint8_t *ptr) : mem_id(ptr[0]), length(ptr[1])  {}
        Entry_t(RTCMemoryManager::RTCMemoryId id, uint8_t len) : mem_id(static_cast<uint8_t>(id)), length(len) {}

        operator bool() const{
            return mem_id != 0 && length != 0;
        }
    };

public:
    static constexpr uint16_t kMemoryLimit = kMemorySize - sizeof(Header_t);
    static constexpr uint16_t kHeaderOffset = kMemorySize - sizeof(Header_t);
    static constexpr uint16_t kHeaderAddress = kHeaderOffset / kBlockSize + kBaseAddress;

    inline static bool _isAligned(size_t len) {
        return (len % kBlockSize) == 0;
    }

    static_assert(sizeof(Header_t) % kBlockSize == 0, "header not aligned");


public:
    static bool _readHeader(Header_t &header);
    static uint8_t *_readMemory(Header_t &header, uint16_t extraSize);

    // copy data to address
    static uint8_t read(RTCMemoryId id, void *data, uint8_t maxSize);
    // reuse temporary allocated memory
    // requires to free the returned pointer using free()
    static uint8_t *read(RTCMemoryId id, uint8_t &length);
    // free pointer returned by read()
    static void free(uint8_t *buffer);

    static bool write(RTCMemoryId id, void *, uint8_t maxSize);

    static bool remove(RTCMemoryId id) {
        return write(id, nullptr, 0);
    }
    static bool clear();

    static bool dump(Print &output, RTCMemoryId id);

    // real time management
    static void setWriteTime(bool enableWriteTime);
    static bool writeTime(bool forceWrite = false);
    static uint32_t readTime(bool set = true);

private:
    static uint8_t *_read(uint8_t *&data, Header_t &header, Entry_t &entry, RTCMemoryId id);

    static bool _enableWriteTime;
};

#include <pop_pack.h>
