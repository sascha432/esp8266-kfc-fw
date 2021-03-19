/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "reset_detector.h"
// #include <ListDir.h>
// #include "plugins_menu.h"
// #include "kfc_fw_config.h"
// #include "WebUIAlerts.h"
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
            __DBG_printf("savecrash sector 0x%04x - 0x%04x (%u)", fs.begin(), fs.end(), fs.end() - fs.begin() + 1);
            return fs;

        #else
            return FlashStorage(SPIFlash::getSector(&_SAVECRASH_start), SPIFlash::getSector((&_SAVECRASH_end));
        #endif
    }

    void clearEEPROM()
    {
        createFlashStorage().format();
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
                __LDBG_printf("sector=0x%04x result=%u size=%u offset=%u header=%u", sector, !!result, result.size(), offset, !!header);
                if (!result || result.size() == 0 || !header) {
                    break; // next sector
                }
                if (offset == 0) {
                    // collect info about sector with offset == 0
                    info._sector_used++;
                    info._space += result.space();
                    info._spaceLargestBlock = std::max(info._spaceLargestBlock, result.space());
                }
                __LDBG_printf("offset=%u time=%u reason=%s stack=%u", offset, header._time, header.getReason(), header.getStackSize());
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
                __LDBG_printf("sector=0x%04x result=%u size=%u offset=%u header=%u", sector, !!result, result.size(), offset, !!header);
                if (!result || result.size() == 0 || !header) {
                    break; // next sector
                }
                __LDBG_printf("time=%u reason=%s stack=%u", header._time, header.getReason(), header.getStackSize());
                callback(CrashLogEntry(header, sector, offset));
                offset += header.getSize();
            }
        }
    }


    void FlashStorage::cut_here(Print &print)
    {
        print.write('\n');
        for (auto i = 0; i < 15; i++ ) {
            print.write('-');
        }
        ets_printf_P(PSTR(" CUT HERE FOR EXCEPTION DECODER "));
        for (auto i = 0; i < 15; i++ ) {
            print.write('-');
        }
        print.write('\n');
    }

    void FlashStorage::print_stack(Print &output, uint32_t *values, uint32_t pos, bool looksLikeStackFrame)
    {
        output.printf_P(PSTR("%08x:  %08x %08x %08x %08x %c\n"), pos, values[0], values[1], values[2], values[3], (looksLikeStackFrame) ? '<':' ');
    }


    bool FlashStorage::printCrashLog(Print &output, const CrashLogEntry &crashLog)
    {
        auto fs = createFlashStorage();
        SPIFlash::FlashResult resultCache;

        // Data header;
        // auto result = read(reinterpret_cast<uint32_t *>(&header), sizeof(header), sector, offset, &resultCache);
        // __LDBG_printf("sector=0x%04x result=%u size=%u offset=%u header=%u", sector, !!result, result.size(), offset, !!header);
        // if (!result.hasData() || !header) {
        //     return false;
        // }

        uint32_t stack[16];
        uint16_t stackSize = crashLog._header._stack.size();
        uint16_t offset = crashLog._offset + sizeof(crashLog._header);
        uint32_t pos = crashLog._header._stack._start;
        while (stackSize) {
            auto read = std::min<uint16_t>(stackSize, sizeof(stack));
            auto result = fs.read(stack, read, crashLog._sector, offset, &resultCache);
            if (!result) {
                return false;
            }

            uint8_t items = result.readSize() / sizeof(*stack);
            std::fill(&stack[items], &stack[16], ~0U);
            for(uint8_t i = 0; i < result.readSize(); i += 4) {
                print_stack(output, &stack[i], pos, crashLog._header._stack._sp == pos + 0x10);
                pos += 0x10;
            }

            offset += read;
            stackSize -= read;
        }

        return true;
    }

}

// ------------------------------------------------------------------------
// custom custom_crash_callback
// ------------------------------------------------------------------------

inline static bool append_crash_data(SaveCrash::FlashStorage &fs, SPIFlash::FlashResult &result, SaveCrash::Data &header)
{

    auto begin = reinterpret_cast<uint32_t *>(header._stack._start);
    auto end = reinterpret_cast<uint32_t *>(header._stack._end);
    header._stack._size = std::min<size_t>(result.space() - sizeof(header), (end - begin) * sizeof(*begin));

    if (!fs.append(header, result)) {
        __LDBG_printf("append failed rsult=%u size=%u", result._result, sizeof(header));
        return false;
    }
    if (!fs.append(begin, header.getStackSize(), result)) {
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
    auto header = SaveCrash::Data(time(nullptr), stack, stack_end, (void *)umm_last_fail_alloc_addr, umm_last_fail_alloc_size, *rst_info);

    header._stack._sp = reinterpret_cast<uint32_t *>(stack)[2]; // TODO verify
    __DBG_printf("sp=%x stack[2]=%x", sp_dump, header._stack._sp);

    auto fs = SaveCrash::createFlashStorage();
    auto results = SPIFlash::FindResultArray();
    uint8_t num;

    // we need at least sizeof(SaveCrash::Data) free space
    if ((num = fs.find(fs.begin(), fs.end(), results, sizeof(SaveCrash::Data))) == 0) {
        ets_printf_P(PSTR("storing the crash log requires at least one empty sector"));
        return;
    }

    __DBG_printf("find returned num=%u results[0]=%x results[1]=%x", num, results[0]._sector, results[1]._sector);

    SPIFlash::FlashResult result;
    if (num == 1) {
        result = fs.init(results[0]._sector);
    }
    else {
        result = fs.copy(results[1]._sector, results[0]._sector);
        if (!result) {
            __LDBG_printf("copy failed %u", result._result);
            return;
        }
    }
    if (!append_crash_data(fs, result, header)) {
        __LDBG_printf("append_crash_data failed %u", result._result);
        return;
    }
    if (!fs.finalize(result)) {
        __LDBG_printf("finalize failed %u", result._result);
        return;
    }
    if (!fs.validate(result)) {
        __LDBG_printf("validate failed %u", result._result);
        return;
    }
    if (num == 2) {
        __LDBG_printf("erase sector %u", results[1]._sector);
        fs.erase(results[1]._sector);
    }
}
