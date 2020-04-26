/**
 * Author: sascha_lammers@gmx.de
 */


#include <Arduino_compat.h>
#include <reset_detector.h>
#include "progmem_data.h"
#include "SaveCrash.h"

namespace SaveCrash {

    uint8_t getCrashCounter()
    {
        uint8_t counter = 0;
        File file = SPIFFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::read);
        if (file) {
            counter = file.read() + 1;
        }
        file = SPIFFS.open(FSPGM(crash_counter_file), fs::FileOpenMode::write);
        file.write(counter);
        return counter;
    }

    void removeCrashCounterAndSafeMode()
    {
        resetDetector.clearCounter();
#if SPIFFS_SUPPORT
        removeCrashCounter();
#endif
    }

    void removeCrashCounter()
    {
#if SPIFFS_SUPPORT
        SPIFFS.begin();
        auto filename = String(FSPGM(crash_counter_file));
        if (SPIFFS.exists(filename)) {
            SPIFFS.remove(filename);
        }
#endif
    }

}

#if DEBUG_HAVE_SAVECRASH

EspSaveCrash espSaveCrash;

namespace SaveCrash {

    void installRemoveCrashCounter(uint32_t delay_seconds)
    {
        Scheduler.addTimer(delay_seconds * 1000UL, false, [](EventScheduler::TimerPtr timer) {
            removeCrashCounter();
            resetDetector.clearCounter();
        });
    }

    void installSafeCrashTimer(uint32_t delay_seconds)
    {
        Scheduler.addTimer(delay_seconds * 1000UL, false, [](EventScheduler::TimerPtr timer) {
            if (espSaveCrash.count()) {
                int num = 0;
                PrintString filename;
                do {
                    filename = PrintString(FSPGM(crash_dump_file), num++);
                } while(SPIFFS.exists(filename));
                auto file = SPIFFS.open(filename, fs::FileOpenMode::write);
                if (file) {
                    espSaveCrash.print(file);
                    if (file) {
                        file.close();
                        espSaveCrash.clear();
                        debug_printf_P(PSTR("Saved crash dump to %s\n"), filename.c_str());
                    }
                }
            }
        });
    }
}

#endif
