/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <EEPROM.h>
#include "SPIFlash.h"

#define DEBUG_SAVE_CRASH                                1

#ifndef DEBUG_SAVE_CRASH
#define DEBUG_SAVE_CRASH                                0
#endif

namespace SaveCrash {

    struct Data {
        static constexpr uint32_t kInvalidUint32 = ~0U;
        static constexpr uint16_t kMaxStackSize = 0x1000;       // limited to less than 4K

        struct LastFailAlloc {
            uint32_t _addr;
            uint32_t _size;
            LastFailAlloc() : _addr(0), _size(0) {}
            LastFailAlloc(void *addr, int size) : _addr(reinterpret_cast<uint32_t>(addr)), _size(static_cast<uint32_t>(size)) {}
        };

        struct Stack {
            uint32_t _start;
            uint32_t _end;
            uint32_t _sp;
            uint32_t _size;

            Stack() : _start(0), _end(0), _sp(0), _size(0) {}
            Stack(uint32_t start, uint32_t end) : _start(start), _end(end), _sp(0), _size(0) {}

            size_t size() const {
                return _size;
            }
            inline static bool valid(uint32_t value) {
                return value != 0 && value != ~0U;
            }
            operator bool() const {
                return valid(_start) && valid(_end) && valid(_end - _start) && valid(_size);
            }
        };

        uint32_t _time;
        Stack _stack;
        LastFailAlloc _lastFailAlloc;
        struct rst_info _info;

        Data() :
            _time(0),
            _stack(),
            _lastFailAlloc(),
            _info({})
        {
        }

        Data(uint32_t time, uint32_t stackStart, uint32_t stackEnd, void *lastFailAllocAddr, int lastFailAllocSize, const struct rst_info &rst_info) :
            _time(time),
            _stack(stackStart, stackEnd),
            _lastFailAlloc(lastFailAllocAddr, lastFailAllocSize),
            _info(rst_info)
        {
        }

        inline operator bool() const {
            return (_stack.size() < kMaxStackSize) && static_cast<bool>(_stack) && (_time != 0) && (_time != kInvalidUint32);
        }

        inline size_t getHeaderSize()  const {
            return sizeof(Data);
        }

        inline size_t getSize() const {
            return _stack.size() + getHeaderSize();
        }

        inline size_t getStackSize() const {
            return _stack.size();
        }

        inline const __FlashStringHelper *getReason() const {
            return ResetDetector::getResetReason((_info.reason));
        }
    };

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
    };

    using ItemCallback = std::function<void(const CrashLogEntry &)>;

    struct FlashStorageInfo {
        uint32_t _size;
        uint32_t _space;
        uint16_t _counter;
        uint16_t _sectors_total;
        uint16_t _sector_used;
        uint16_t _spaceLargestBlock;

        FlashStorageInfo() :
            _size(0),
            _space(0),
            _counter(0),
            _sectors_total(0),
            _sector_used(0),
            _spaceLargestBlock(0)
        {
        }
    };

    class FlashStorage : public SPIFlash::FlashStorage {
    public:
        using SPIFlash::FlashStorage::FlashStorage;

        FlashStorageInfo getInfo();
        void getCrashLog(ItemCallback callback);
        bool printCrashLog(Print &output, const CrashLogEntry &crashLog);

    protected:
        static void cut_here(Print &print);
        static void print_stack(Print &output, uint32_t *values, uint32_t pos, bool looksLikeStackFrame);
    };

    FlashStorage createFlashStorage();
    void clearEEPROM();
    uint8_t getCrashCounter();
    void removeCrashCounterAndSafeMode();
    void removeCrashCounter(); // calling SPIFFS.begin()
    void installRemoveCrashCounter(uint32_t delay_seconds);

};

extern "C" void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end);
