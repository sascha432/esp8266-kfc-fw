/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "reset_detector.h"
#include "SPIFlash.h"
#include "save_crash.h"
#include "section_defines.h"

#if DEBUG_SAVE_CRASH
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern "C" {

    // cores/esp8266/heap.cpp
    extern void *umm_last_fail_alloc_addr;
    extern int umm_last_fail_alloc_size;

}

static void ets_printf_P(const char *str, ...) {
    char destStr[160];
    char *c = destStr;
    va_list argPtr;
    va_start(argPtr, str);
    vsnprintf(destStr, sizeof(destStr), str, argPtr);
    va_end(argPtr);
    while (*c) {
        ets_uart_putc1(*(c++));
    }
}

#define SAVECRASH_EXCEPTION_FMT                     "epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x"
#define SAVECRASH_EXCEPTION_ARGS(rst_info)          rst_info.exccause, rst_info.epc1, rst_info.epc2, rst_info.epc3, rst_info.excvaddr, rst_info.depc

namespace SaveCrash {

    uint8_t getCrashCounter()
    {
#if KFC_DISABLE_CRASHCOUNTER
        return 0;
#else
        uint8_t counter = 0;
        File file = KFCFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::read);
        if (file) {
            counter = file.read() + 1;
        }
        file = KFCFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::write);
        file.write(counter);
        return counter;
#endif
    }

    void removeCrashCounterAndSafeMode()
    {
        resetDetector.clearCounter();
#if KFC_DISABLE_CRASHCOUNTER == 0
        removeCrashCounter();
#endif
    }

    void removeCrashCounter()
    {
#if KFC_DISABLE_CRASHCOUNTER == 0
        KFCFS.begin();
        auto filename = String(FSPGM(crash_counter_file, "/.pvt/crash_counter"));
        if (KFCFS.exists(filename)) {
            KFCFS.remove(filename);
        }
#endif
    }

    void installRemoveCrashCounter(uint32_t delay_seconds)
    {
        _Scheduler.add(Event::seconds(delay_seconds), false, [](Event::CallbackTimerPtr timer) {
#if KFC_DISABLE_CRASHCOUNTER == 0
            removeCrashCounter();
#endif
            resetDetector.clearCounter();
        });
    }

    FlashStorage createFlashStorage()
    {
        #if DEBUG_SAVE_CRASH
            auto fs = FlashStorage(SPIFlash::getSector(&_SAVECRASH_start), SPIFlash::getSector(&_SAVECRASH_end));
            __DBG_printf("savecrash sector %x - %x (%u)", fs.begin(), fs.end(), fs.end() - fs.begin() + 1);
            return fs;

        #else
            return FlashStorage(SPIFlash::getSector(&_SAVECRASH_start), SPIFlash::getSector(&_SAVECRASH_end));
        #endif
    }

    bool clearStorage(ClearStorageType type, uint32_t options)
    {
        return createFlashStorage().clear(type, options);
    }

#if ESP8266

    inline static const __FlashStringHelper *getExceptionFPStr(uint32_t execption) {
        switch(execption) {
            case 0: return F("IllegalInstructionCause");
            case 1: return F("SyscallCause");
            case 2: return F("InstructionFetchErrorCause");
            case 3: return F("LoadStoreErrorCause");
            case 4: return F("Level1InterruptCause");
            case 5: return F("AllocaCause");
            case 6: return F("IntegerDivideByZeroCause");
            case 7: return F("Reserved for Tensilica");
            case 8: return F("PrivilegedCause");
            case 9: return F("LoadStoreAlignmentCause");
            case 12: return F("InstrPIFDateErrorCause");
            case 13: return F("LoadStorePIFDataErrorCause");
            case 14: return F("InstrPIFAddrErrorCause");
            case 15: return F("LoadStorePIFAddrErrorCause");
            case 16: return F("InstTLBMissCause");
            case 17: return F("InstTLBMultiHitCause");
            case 18: return F("InstFetchPrivilegeCause");
            case 19: return F("Reserved for Tensilica");
            case 20: return F("InstFetchProhibitedCause");
            case 24: return F("LoadStoreTLBMissCause");
            case 25: return F("LoadStoreTLBMultiHitCause");
            case 26: return F("LoadStorePrivilegeCause");
            case 27: return F("Reserved for Tensilica");
            case 28: return F("LoadProhibitedCause");
            case 29: return F("StoreProhibitedCause");
            default:
                break;
        }
        return nullptr;
    }

    void Data::printReason(Print &output) const
    {
        auto ptr = _info.reason == REASON_EXCEPTION_RST ? getExceptionFPStr(_info.exccause) : nullptr;
        if (ptr) {
            output.print(ptr);
        }
        else {
            output.print(ResetDetector::getResetReason(_info.reason));
            if (_info.reason == REASON_EXCEPTION_RST) {
                output.printf_P(PSTR(" (%u)"), _info.exccause);
            }
        }
    }

#else

    void Data::printReason(Print &output) const
    {
        output.print(ResetDetector::getResetReason(_info.reason))
        output.printf_P(PSTR(" (%u)"), _info.exccause);
    }

#endif

    void Data::printMD5(Print &output) const {
        auto ptr = _md5;
        auto endPtr = ptr + 16;
        while (ptr < endPtr) {
            output.printf_P(PSTR("%02x"), (uint32_t)*ptr++);
        }
    }

    bool Data::setMD5(const char *str) {
        if (strlen(str) != 32) {
            std::fill(std::begin(_md5), std::end(_md5), 0);
            return false;
        }
        hex2bin(_md5, sizeof(_md5), str);
        return true;
    }

    FlashStorageInfo FlashStorage::getInfo(ItemCallback callback)
    {
        Data header;
        FlashStorageInfo info;

        for(auto sector = begin(); sector <= end(); sector++) {
            uint16_t offset = 0;
            SPIFlash::FlashResult resultCache;
            info._sectors_total++;
            for(;;) {
                auto result = read(reinterpret_cast<uint32_t *>(&header), sizeof(header), sector, offset, &resultCache);
                // __LDBG_printf("sector=%x magic=%08x hdr.size=%u result=%u size=%u offset=%u header=%u", sector, result._header._magic, result._header._size, (bool)result, result.size(), offset, (bool)header);
                if (!result || result.size() == 0 || !header) {
                    if (offset == 0) {
                        info._spaceLargestBlock = std::max<uint16_t>(info._spaceLargestBlock, kMaxDataSize);
                    }
                    break; // next sector
                }
                if (offset == 0) {
                    // collect info about sector with offset == 0
                    info._sectors_used++;
                    if (result.space() >= sizeof(header)) {
                        info._spaceLargestBlock = std::max<uint16_t>(info._spaceLargestBlock, result.space());
                    }
                }
                // __LDBG_printf("offset=%u time=%u reason=%s stack=%u", offset, header._time, header.getReason(), header.getStackSize());
                if (callback && callback(CrashLogEntry(header, sector, offset)) == false) {
                    callback = nullptr;
                }
                offset += header.getSize();
                info._size += header.getSize();
                info._counter++;
            }
        }
        return info;
    }

    void FlashStorage::getCrashLog(ItemCallback callback)
    {
        for(auto sector = begin(); sector <= end(); sector++) {
            uint16_t offset = 0;
            SPIFlash::FlashResult resultCache;
            for(;;) {
                Data header;
                auto result = read(reinterpret_cast<uint32_t *>(&header), sizeof(header), sector, offset, &resultCache);
                __LDBG_printf("sector=%x magic=%08x hdr.size=%u result=%u size=%u offset=%u header=%u", sector, result._header._magic, result._header._size, (bool)result, result.size(), offset, (bool)header);
                if (!result || result.size() == 0 || !header) {
                    break; // next sector
                }
                __LDBG_printf("time=%u size=%u stack=%u start=0x%08x end=0x%08x sp=0x%08x reason=%s cause=%u " SAVECRASH_EXCEPTION_FMT " md5=%s",
                    header._time,
                    header.getSize(),
                    header.getStackSize(),
                    header._stack._begin,
                    header._stack._end,
                    header._stack._sp,
                    header.getReason(),
                    SAVECRASH_EXCEPTION_ARGS(header._info),
                    header.getMD5().c_str()
                );

                if (callback(CrashLogEntry(header, sector, offset)) == false) {
                    return;
                }
                offset += header.getSize();
            }
        }
    }

    void FlashStorage::printCrashLogEntry(Print &output, const SaveCrash::CrashLogEntry &item)
    {
        output.print(F(" reason="));
        item._header.printReason(output);
        // output.print(item._header.getReason());
        if (item._header._info.reason == REASON_EXCEPTION_RST) {
            output.printf_P(PSTR(" " SAVECRASH_EXCEPTION_FMT), SAVECRASH_EXCEPTION_ARGS(item._header._info));
        }

        auto version = item._header.getVersion();
        if (version) {
            output.printf_P(PSTR(" version=%s\n"), version.toString().c_str());
        }
        output.printf_P(PSTR(" md5=%s stack-size=%u\n"), item._header.getMD5().c_str(), item._header.getStackSize());
    }

    void FlashStorage::cut_here(Print &output)
    {
        output.write('\n');
        for (auto i = 0; i < 15; i++ ) {
            output.write('-');
        }
        output.printf_P(PSTR(" CUT HERE FOR EXCEPTION DECODER "));
        for (auto i = 0; i < 15; i++ ) {
            output.write('-');
        }
        output.write('\n');
    }

    void FlashStorage::print_stack(Print &output, uint32_t *values, size_t items, uint32_t &pos, const Data &header)
    {
        for(uint8_t i = 0; i < items / sizeof(values[0]); i++) {
            output.printf_P(PSTR("%08x:  %08x %08x %08x %08x %c\n"), pos, values[0], values[1], values[2], values[3], false ? '<' : ' ');
            values += 4;
            pos += 0x10;
            if (pos > header._stack._end) {
                break;
            }
        }
    }


    bool FlashStorage::printCrashLog(Print &output, const CrashLogEntry &crashLog)
    {
        SPIFlash::FlashResult resultCache;

        auto &header = crashLog._header;
        auto &stack = header._stack;
        auto &rst_info = header._info;

        uint16_t stackSize = header._stack.size();
        uint16_t offset = crashLog._offset + sizeof(header);
        auto pos = (uint32_t)header._stack._begin;

        cut_here(output);
        output.print(F("\nFirmware "));
        header.printVersion(output);
        output.print(F("\nMD5 "));
        header.printMD5(output);
        output.print(F("\nTimestamp "));
        output.print(header.getTimeStr());
        output.print('\n');

        if (static_cast<int>(rst_info.reason) == 254) {
            output.print(F("\nUser exception (panic/abort/assert)\n"));
        }
        else if (rst_info.reason == REASON_EXCEPTION_RST) {
            output.printf_P(PSTR("\nException (%d):\n" SAVECRASH_EXCEPTION_FMT "\n"), SAVECRASH_EXCEPTION_ARGS(rst_info));
        }
        else if (rst_info.reason == REASON_SOFT_WDT_RST) {
            output.print(F("\nSoft WDT reset\n"));
        }
        else {
            output.print(F("\nGeneric Reset\n"));
        }

        output.printf_P(PSTR("\n>>>stack>>>\nsp: %08x end: %08x offset: %04x\n"), stack._sp, stack._end, stack._begin - stack._sp);

        while (stackSize) {
            uint32_t stackDump[16 * 4]; // multiples of 4
            auto readLen = std::min<size_t>(stackSize, sizeof(stackDump));
            auto result = read(&stackDump[0], readLen, crashLog._sector, offset, &resultCache);
            if (!result) {
                __LDBG_printf("sector=%x result=%u read_size=%u offset=%u read failed", crashLog._sector, !!result, readLen, offset);
                return false;
            }

            // __LDBG_printf("pos=%08x items=%u stacksize=%u", pos, result.readSize() / sizeof(stackDump[0]), header.getStackSize());
            print_stack(output, &stackDump[0], result.readSize() / sizeof(stackDump[0]), pos, crashLog._header);

            offset += readLen;
            stackSize -= readLen;
        }

        output.print(F("<<<stack<<<\n"));

        if (header._lastFailAlloc._addr) {
            output.printf_P(PSTR("\nlast failed alloc call: %08X(%d)\n"), header._lastFailAlloc._addr, header._lastFailAlloc._size);
        }

        cut_here(output);

        return true;
    }

}

// ------------------------------------------------------------------------
// custom custom_crash_callback
// ------------------------------------------------------------------------

inline static bool append_crash_data(SaveCrash::FlashStorage &fs, SPIFlash::FlashResult &result, SaveCrash::Data &header)
{
    // limit stack size to available storage space
    header._stack._size = std::min<uint32_t>(result.space() - sizeof(header), header._stack._size);
    __LDBG_printf("APPEND_CRASH_DATA %u", header._stack._size);

    if (!fs.append(header, result)) {
        __LDBG_printf("append failed rsult=%u size=%u", result._result, sizeof(header));
        return false;
    }

    // auto stack = header._stack._begin;
    // SaveCrash::FlashStorage::print_stack(Serial, (uint32_t *)header._stack._begin, 16, stack, header);

    if (!fs.append(reinterpret_cast<uint32_t *>(header._stack._begin), header.getStackSize(), result)) {
        __LDBG_printf("append failed result=%u size=%u", result._result, header.getStackSize());
        return false;
    }

    return true;
}

#if 1

#if IOT_LED_MATRIX_OUTPUT_PIN
#include "NeoPixelEspEx.h"
#endif

inline __attribute__((__always_inline__)) static void _custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end)
{
    register uint32_t sp asm("a1");
    uint32_t sp_dump = sp;

    // create header first to capture umm_last_fail_alloc_*
    auto header = SaveCrash::Data(time(nullptr), stack, stack_end, sp_dump, (void *)umm_last_fail_alloc_addr, umm_last_fail_alloc_size, *rst_info);

#if IOT_LED_MATRIX_OUTPUT_PIN
    NeoPixelEx::forceClear(IOT_LED_MATRIX_OUTPUT_PIN, IOT_CLOCK_NUM_PIXELS);
    pinMode(IOT_LED_MATRIX_OUTPUT_PIN, INPUT);
#endif

    auto fs = SaveCrash::createFlashStorage();
    auto results = SPIFlash::FindResultArray();
    uint8_t num;

    // try to find enough space for the entire stack trace
    num = fs.find(fs.begin(), fs.end(), results, header.getSize());
    if (num == 0) {
        // we need at least sizeof(SaveCrash::Data) free space
        if ((num = fs.find(fs.begin(), fs.end(), results, sizeof(SaveCrash::Data))) == 0) {
            ets_printf_P(PSTR("storing the crash log requires at least one empty sector"));
            return;
        }
    }

    SPIFlash::FlashResult result;
    if (num == 1) {
        result = fs.init(results[0]._sector);
        if (!result) {
            ets_printf_P(PSTR("initializing sector %x failed"), results[0]._sector);
            return;
        }
    }
    else {
        result = fs.copy(results[1]._sector, results[0]._sector);
        if (!result) {
            ets_printf_P(PSTR("copying sector %x to %x failed"), results[1]._sector, results[0]._sector);
            return;
        }
    }
    if (!append_crash_data(fs, result, header)) {
        ets_printf_P(PSTR("appending data to sector=%x failed=%d"), results[0]._sector, result._result);
        return;
    }
    if (!fs.finalize(result)) {
        ets_printf_P(PSTR("storing final data on sector=%x failed=%d"), results[0]._sector, result._result);
        return;
    }
    if (!fs.validate(result)) {
        ets_printf_P(PSTR("validating of written sector=%x failed=%d"), results[0]._sector, result._result);
        return;
    }
    if (num == 2) {
        result = fs.erase(results[1]._sector);
        if (result != SPIFlash::FlashResultType::SUCCESS) {
            ets_printf_P(PSTR("erasing sector=%x failed=%d"), results[1]._sector, result._result);
        }

    }
}

void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end)
{
    _custom_crash_callback(rst_info, stack, stack_end);
}

#endif
