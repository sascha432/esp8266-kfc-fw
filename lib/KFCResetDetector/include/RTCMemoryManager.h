/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <push_pack.h>

#if HAVE_KFC_FIRMWARE_VERSION
#include <EventScheduler.h>
#include <PluginComponent.h>
#endif

#ifndef DEBUG_RTC_MEMORY_MANAGER
#    define DEBUG_RTC_MEMORY_MANAGER 0
// #    define DEBUG_RTC_MEMORY_MANAGER (0 || defined(DEBUG_ALL))
#endif

#if DEBUG_RTC_MEMORY_MANAGER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#include <Utility/ProgMemHelper.h>

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

    #if ESP8266
        // static constexpr uint16_t kMemoryLimit = kMemorySize - sizeof(Header_t);
        static constexpr uint8_t kBlockSize = 4;
        static constexpr uint8_t kBaseAddress = 64;
        static constexpr uint16_t kMemorySize = (128 - kBaseAddress) * kBlockSize; // fixed
        static constexpr uint8_t kLastAddress = kBaseAddress + (kMemorySize / kBlockSize) - 1;
        static constexpr uint32_t kClearNumBlocks = 16;
    #elif ESP32
        static constexpr uint8_t kBaseAddress = 0;
        static constexpr uint16_t kMemorySize = (64 * 4) - kBaseAddress; // can be adjusted
        static constexpr uint8_t kBlockSize = 1;
        static constexpr uint8_t kLastAddress = kBaseAddress + (kMemorySize / kBlockSize) - 1;
        static constexpr uint32_t kClearNumBlocks = 64;
    #endif

    struct Header_t {
        // length includes the header
        uint16_t length;
        uint16_t crc;
        // get length of the data
        inline uint16_t data_length() const {
            return length - sizeof(Header_t);
        }
        // get offset of the crc
        inline uint16_t crc_offset() const {
            return data_length() + offsetof(Header_t, crc);
        }
        // get start address
        inline uint16_t start_address() const {
            return ((kMemorySize - length) / kBlockSize) + kBaseAddress;
        }
        // get start pointer to the start of the data
        inline uint8_t *begin(void *ptr) const {
            return reinterpret_cast<uint8_t *>(ptr);
        }
        // get pointer to the end of the data
        inline uint8_t *end(void *ptr) const {
            return begin(ptr) + data_length();
        }
        // get distance to the start in bytes
        inline size_t distance(void *start, void *iterator) {
            return end(iterator) - begin(start);
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
    enum class SyncStatus : uint8_t {
        NO = 0,
        YES,
        UNKNOWN,
        NTP_UPDATE
    };

    struct RtcTime {
        #if RTC_SUPPORT == 0
            uint32_t time;
        #endif
        SyncStatus status;

        RtcTime(time_t _time = 0, SyncStatus _status = SyncStatus::UNKNOWN) :
            #if RTC_SUPPORT == 0
                time(_time),
            #endif
            status(_status)
        {
        }

        uint32_t getTime() const {
            #if RTC_SUPPORT
                return 0;
            #else
                return time;
            #endif
        }

        static const __FlashStringHelper *getStatus(SyncStatus status) {
            switch(status) {
            case SyncStatus::YES:
                return F("In sync");
            case SyncStatus::NO:
                return F("Out of sync");
            case SyncStatus::NTP_UPDATE:
                return F("NTP update in progress");
            default:
                break;
            }
            return F("Unknown");
        }

        const __FlashStringHelper *getStatus() const {
            return getStatus(status);
        }
    };

    static constexpr auto kRtcTimeSize = sizeof(RtcTime);

public:
    static constexpr uint16_t kHeaderOffset = kMemorySize - sizeof(Header_t);
    static constexpr uint16_t kHeaderAddress = (kHeaderOffset / kBlockSize) + kBaseAddress;
    static constexpr uint16_t kMemoryLimit = kMemorySize - (kMemorySize - (kHeaderAddress * kBlockSize)) - kBaseAddress;

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
    static void release(uint8_t *buffer);

    static bool write(RTCMemoryId id, const void *, uint8_t maxSize);

    static bool remove(RTCMemoryId id) {
        return write(id, nullptr, 0);
    }
    static bool clear();

    static bool dump(Print &output, RTCMemoryId id);

private:
    static uint8_t *_read(uint8_t *&data, Header_t &header, Entry_t &entry, RTCMemoryId id);

// methods to use the internal RTC
public:
    #if RTC_SUPPORT == 0
        static void setupRTC();
        static void updateTimeOffset(uint32_t millis_offset);
        static void storeTime();
    #endif
    static void setTime(time_t time, SyncStatus status);
    static SyncStatus getSyncStatus();
    // there must be valid record in the RTC memory to use following methods
    static void setSyncStatus(bool inSync);
    static void setNtpUpdate();

    inline static RtcTime readTime() {
        return _readTime();
    }

private:
    static RtcTime _readTime();
    static void _writeTime(const RtcTime &time);
    static void _clearTime();

    // TODO remove
public:
    static uint32_t _bufSize;

    #if RTC_SUPPORT == 0
    public:
        class RtcTimer : public OSTimer {
        public:
            RtcTimer() : OSTimer(OSTIMER_NAME("RtcTimer")) {}

            virtual void run() {
                RTCMemoryManager::storeTime();
            }
        };

        static RtcTimer _rtcTimer;
    #endif
};

inline RTCMemoryManager::SyncStatus RTCMemoryManager::getSyncStatus()
{
    return _readTime().status;
}

inline void RTCMemoryManager::setTime(time_t time, SyncStatus status)
{
    auto rtc = RtcTime(time, status);
    __LDBG_printf("set time=%u status=%s", rtc.getTime(), rtc.getStatus());
    _writeTime(rtc);
}

#if RTC_SUPPORT == 0

    inline void RTCMemoryManager::storeTime()
    {
        auto rtc = _readTime();
        rtc.time = time(nullptr);
        _writeTime(rtc);
    }

#endif

inline void RTCMemoryManager::setSyncStatus(bool status)
{
    auto rtc = _readTime();
    __LDBG_printf("new status=%u old status=%u", status, rtc.status);
    if (
        (status == true && rtc.status != SyncStatus::YES) ||
        (status == false && rtc.status == SyncStatus::YES)
    ) {
        rtc.status = status ? SyncStatus::YES : SyncStatus::NO;
        _writeTime(rtc);
    }
}

inline void RTCMemoryManager::setNtpUpdate()
{
    auto rtc = _readTime();
    rtc.status = SyncStatus::NTP_UPDATE;
    _writeTime(rtc);
}

#include <pop_pack.h>

#if DEBUG_RTC_MEMORY_MANAGER
#include "debug_helper_disable.h"
#endif
