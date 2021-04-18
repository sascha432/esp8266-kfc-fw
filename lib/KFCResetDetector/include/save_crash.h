/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <EEPROM.h>
#include "SPIFlash.h"
#include "coredecls.h"

// #define DEBUG_SAVE_CRASH                                    1

#ifndef DEBUG_SAVE_CRASH
#define DEBUG_SAVE_CRASH                                    0
#endif

#ifndef KFC_DISABLE_CRASHCOUNTER
#define KFC_DISABLE_CRASHCOUNTER                          0
#endif

#if DEBUG_SAVE_CRASH
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


class HttpHeaders;
class AsyncWebServerResponse;
class AsyncWebServerRequest;

// src/kfc_fw_config_forms.cpp
extern const char *getFirmwareMD5();

namespace SaveCrash {

    // limit stack trace
    static constexpr uint16_t kLimitStackTraceSize = 1200;

    inline static uint16_t limitStackTraceSize(uint32_t begin, uint32_t end) {
        __LDBG_printf("begin=%p end=%p end-begin=%d return=%d", begin, end, end-begin,std::min<int>(SaveCrash::kLimitStackTraceSize, end - begin));
        return std::min<size_t>(SaveCrash::kLimitStackTraceSize, end - begin);
    }

    using ClearStorageType = SPIFlash::ClearStorageType;

    class Data {
    public:
        static constexpr uint32_t kInvalidUint32 = ~0U;

        struct FirmwareVersion {
            union {
                struct {
                    uint32_t major: 5;
                    uint32_t minor: 5;
                    uint32_t revision: 6;
                    uint32_t build: 16;
                };
                uint32_t __version;
            };

            FirmwareVersion(); // kfc_fw_config.cpp
            FirmwareVersion(uint32_t version) : __version(version) {}

            inline operator uint32_t() const {
                return __version;
            }

            inline operator bool() const {
                return __version != 0 && __version != ~0U;
            }

            String toString(const __FlashStringHelper *buildSep = getDefaultBuildSeperator()) const; // kfc_fw_config.cpp
            void printTo(Print &output, const __FlashStringHelper *buildSep = getDefaultBuildSeperator()) const; // kfc_fw_config.cpp

        private:
            inline static const __FlashStringHelper *getDefaultBuildSeperator() {
                return F(".b");
            }
        };

        static_assert(sizeof(FirmwareVersion) == sizeof(uint32_t), "invalid size");

        struct LastFailAlloc {
            uint32_t _addr;
            uint32_t _size;
            LastFailAlloc() :
                _addr(0),
                _size(0)
            {
            }

            LastFailAlloc(void *addr, int size) :
                _addr(reinterpret_cast<uint32_t>(addr)),
                _size(static_cast<uint32_t>(size))
            {
            }
        };

        struct Stack {
            uint32_t _begin;
            uint32_t _end;
            uint32_t _sp;
            uint32_t _size;

            Stack() :
                _begin(0),
                _end(0),
                _sp(0),
                _size(0)
            {
            }

            Stack(uint32_t start, uint32_t end, uint32_t sp = 0) :
                _begin(start),
                _end(end),
                _sp(sp),
                _size(limitStackTraceSize(_begin, _end))
            {
            }

            inline size_t size() const {
                return _size;
            }

            inline static bool valid(uint32_t value) {
                return value != 0 && value != ~0U;
            }

            inline static bool valid(uint32_t *value) {
                return valid(reinterpret_cast<uint32_t>(value));
            }

            operator bool() const {
                return valid(_begin) && valid(_end) && valid(_size);
            }
        };

        uint32_t _time;
        Stack _stack;
        LastFailAlloc _lastFailAlloc;
        uint32_t _fwVersion;
        struct rst_info _info;
        uint8_t _md5[16];

        Data() :
            _time(0),
            _stack(),
            _lastFailAlloc(),
            _fwVersion(FirmwareVersion()),
            _info({})
        {
            setMD5(getFirmwareMD5());
        }

        Data(uint32_t time, uint32_t stackStart, uint32_t stackEnd, uint32_t sp, void *lastFailAllocAddr, int lastFailAllocSize, const struct rst_info &rst_info) :
            _time(time),
            _stack(stackStart, stackEnd, sp),
            _lastFailAlloc(lastFailAllocAddr, lastFailAllocSize),
            _fwVersion(FirmwareVersion()),
            _info(rst_info)
        {
            setMD5(getFirmwareMD5());
        }

        operator bool() const;

        inline const size_t getHeaderSize() const {
            return sizeof(Data);
        }

        inline size_t getSize() const {
            return _stack.size() + getHeaderSize();
        }

        inline size_t getStackSize() const {
            return _stack.size();
        }

        inline String getStack() const {
            return PrintString(F("0x%08x - 0x%08x"), _stack._begin, _stack._end);
        }

        inline String getTimeStr() const {
            PrintString timeStr;
            timeStr.strftime(FSPGM(strftime_date_time_zone), static_cast<time_t>(_time));
            return timeStr;
        }

        inline const __FlashStringHelper *getReason() const {
            return ResetDetector::getResetReason(_info.reason);
        }

        inline FirmwareVersion getVersion() const {
            return FirmwareVersion(_fwVersion);
        }

        // does not allocate memory
        inline void printVersion(Print &output) const {
            getVersion().printTo(output);
        }

        String getMD5() const {
            String md5Str;
            if (md5Str.reserve(32)) {
                bin2hex_append(md5Str, _md5, sizeof(_md5));
            }
            return md5Str;
        }

        // does not allocate memory
        void printReason(Print &output) const;
        void printMD5(Print &output) const;
        bool setMD5(const char *str);
    };

    static constexpr auto kDataSize = sizeof(Data);
    static constexpr auto kMaxDataSize = SPIFlash::kSectorMaxSize - kDataSize;

    inline Data::operator bool() const
    {
        return (_stack.size() <= kMaxDataSize) && static_cast<bool>(_stack) && (_time != 0) && (_time != kInvalidUint32);
    }

    struct CrashLogEntry {
        const Data &_header;
        uint16_t _sector;
        uint16_t _offset;

        CrashLogEntry(const Data &header, uint16_t sector, uint16_t offset) :
            _header(header),
            _sector(sector),
            _offset(offset)
        {
        }

        inline uint32_t getId() const {
            return crc32(_header._md5, sizeof(_header._md5), (_sector << 12) | _offset);
        }
    };

    // return true to continue, false to abort
    using ItemCallback = std::function<bool(const CrashLogEntry &)>;

    class FlashStorage;

    class FlashStorageInfo {
    public:
        FlashStorageInfo() :
            _size(0),
            _spaceLargestBlock(0),
            _counter(0),
            _sectors_total(0),
            _sectors_used(0)
        {
        }

        inline uint8_t numTraces() const {
            return _counter;
        }

        inline uint16_t getLargestBlock() const {
            return _spaceLargestBlock;
        }

        // free space
        inline uint32_t available() const {
            return std::max<int>(0, capacity() - size() - (std::min(1, _sectors_total - 1) * (SPIFlash::kSectorMaxSize - kMaxDataSize)));
        }

        // max capacity
        inline uint32_t capacity() const {
            return _sectors_total > 1 ? (_sectors_total - 1) * kMaxDataSize : kMaxDataSize;
        }

        // space used
        inline uint32_t size() const {
            return _size;
        }

    private:
        friend FlashStorage;

        uint32_t _size;
        uint16_t _spaceLargestBlock;
        uint8_t _counter;
        uint8_t _sectors_total;
        uint8_t _sectors_used;
    };

    class FlashStorage : public SPIFlash::FlashStorage {
    public:
        using SPIFlash::FlashStorage::FlashStorage;

        // same as getCrashLog() but returns FlashStorageInfo
        FlashStorageInfo getInfo(ItemCallback callback = nullptr);
        void getCrashLog(ItemCallback callback);
        bool printCrashLog(Print &output, const CrashLogEntry &crashLog);
        void printCrashLogEntry(Print &output, const SaveCrash::CrashLogEntry &item);

        static void cut_here(Print &output);
        static void print_stack(Print &output, uint32_t *values, size_t items, uint32_t &pos, const Data &header);
    };

    FlashStorage createFlashStorage();
    bool clearStorage(ClearStorageType type, uint32_t options = 50);
    uint8_t getCrashCounter();
    void removeCrashCounterAndSafeMode();
    void removeCrashCounter(); // calling KFCFS.begin()
    void installRemoveCrashCounter(uint32_t delay_seconds);

    struct webHandler {

        // in web_server.cpp
        static AsyncWebServerResponse *json(AsyncWebServerRequest *request, HttpHeaders &httpHeaders);

    };

};

extern "C" void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end);

#include <debug_helper_disable.h>
