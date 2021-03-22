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

    // cores/esp8266/core_esp8266_postmortem.cpp
    void ___static_ets_printf_P(const char *str, ...);
    #define ets_printf_P ___static_ets_printf_P

}

#define SAVECRASH_EXCEPTION_FMT                     "epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x"
#define SAVECRASH_EXCEPTION_ARGS(rst_info)          rst_info.exccause, rst_info.epc1, rst_info.epc2, rst_info.epc3, rst_info.excvaddr, rst_info.depc

namespace SaveCrash {

    uint8_t getCrashCounter()
    {
        uint8_t counter = 0;
        File file = KFCFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::read);
        if (file) {
            counter = file.read() + 1;
        }
        file = KFCFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::write);
        file.write(counter);
        return counter;
    }

    void removeCrashCounterAndSafeMode()
    {
        resetDetector.clearCounter();
        removeCrashCounter();
    }

    void removeCrashCounter()
    {
        KFCFS.begin();
        auto filename = String(FSPGM(crash_counter_file, "/.pvt/crash_counter"));
        if (KFCFS.exists(filename)) {
            KFCFS.remove(filename);
        }
    }

    void installRemoveCrashCounter(uint32_t delay_seconds)
    {
        _Scheduler.add(Event::seconds(delay_seconds), false, [](Event::CallbackTimerPtr timer) {
            removeCrashCounter();
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

    FlashStorageInfo FlashStorage::getInfo()
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
                        info._space += SPIFlash::kSectorMaxSize;
                        info._spaceLargestBlock = std::max(info._spaceLargestBlock, SPIFlash::kSectorMaxSize);
                    }
                    break; // next sector
                }
                if (offset == 0) {
                    // collect info about sector with offset == 0
                    info._sector_used++;
                    info._space += result.space();
                    info._spaceLargestBlock = std::max(info._spaceLargestBlock, result.space());
                }
                // __LDBG_printf("offset=%u time=%u reason=%s stack=%u", offset, header._time, header.getReason(), header.getStackSize());
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
                __LDBG_printf("time=%u size=%u reason=%s stack=%u start=0x%08x end=0x%08x sp=0x%08x reason=%s cause=%u " SAVECRASH_EXCEPTION_FMT " md5=%s",
                    header._time,
                    header.getSize(),
                    header.getReason(),
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
        output.print(item._header.getReason());
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
                __DBG_printf("pos OUT OF RANGE idx=%u offset=%u", i, (pos > header._stack._end));
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
        output.print(F(" MD5 "));
        header.printMD5(output);
        output.print(F("\n"));

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

        output.print(F("\n>>>stack>>>\n"));

        // if (reinterpret_cast<uint32_t>(stack._end) == 0x3fffffb0) {
        //     output.print(F("\nctx: cont\n"));
        // }
        // else {
        //     output.print(F("\nctx: sys\n"));
        // }

        output.printf_P(PSTR("sp: %08x end: %08x offset: %04x\n"), stack._sp, stack._end, stack._begin - stack._sp);

        while (stackSize) {
            uint32_t stackDump[16 * 4]; // multiples of 4
            auto readLen = std::min<size_t>(stackSize, sizeof(stackDump));
            auto result = read(&stackDump[0], readLen, crashLog._sector, offset, &resultCache);
            if (!result) {
                __LDBG_printf("sector=%x result=%u read_size=%u offset=%u read failed", crashLog._sector, !!result, readLen, offset);
                return false;
            }

            // __LDBG_printf("READIN STACK ofs=%x size=%u", SPIFlash::FlashResult::dataOffset(result._sector) + offset);

            // removed since it seems to be 16 byte aligned
            // fill stack if not read completely
            // size_t items = result.readSize() / sizeof(stackDump[0]);
            // std::fill(&stackDump[items], std::end(stackDump), ~0U);

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
    __DBG_printf("APPEND_CRASH_DATA %u", header._stack._size);

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

void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end)
{
    register uint32_t sp asm("a1");
    uint32_t sp_dump = sp;

    // create header first to capture umm_last_fail_alloc_*
    auto header = SaveCrash::Data(time(nullptr), stack, stack_end, sp_dump, (void *)umm_last_fail_alloc_addr, umm_last_fail_alloc_size, *rst_info);

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
        if (!result) {
            ets_printf_P(PSTR("erasing sector=%x failed=%d"), results[1]._sector, result._result);
        }

    }
}
