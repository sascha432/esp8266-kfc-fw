/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <PrintString.h>
#include "plugins.h"
#include "reset_detector.h"
#include "save_crash.h"

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#define LIST_SAVE_CASH_COMMANDS "info|list|print|clear|format"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(RD, "RD", "Reset detector clear counter", "Display information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SAVECRASH, "SAVECRASH", "<" LIST_SAVE_CASH_COMMANDS ">[,<number>|<clear-type=erase,shrink,magic,version>]", "Manage SaveCrash");

/*

// display info
+RD?
// clear counter
+RD

// clear all crash logs
+SAVECRASH=clear
// list crash logs
+SAVECRASH=list
// display first crash log
+SAVECRASH=print,1




+panic
+savecrash=info
+savecrash=list
+savecrash=print,1

*/

ATModeCommandHelpArrayPtr ResetDetectorPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND(RD), PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASH) };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool ResetDetectorPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RD))) {
        if (args.isQueryMode()) {
            args.getStream().printf_P(PSTR("safe mode: %u\nreset counter: %u\ninitial reset counter: %u\ncrash: %u\nreboot: %u\nreset: %u\nwake up: %u\nreset reason: %s\n"),
                resetDetector.getSafeMode(),
                resetDetector.getResetCounter(),
                resetDetector.getInitialResetCounter(),
                resetDetector.hasCrashDetected(),
                resetDetector.hasRebootDetected(),
                resetDetector.hasResetDetected(),
                resetDetector.hasWakeUpDetected(),
                resetDetector.getResetReason());
        }
        else {
            resetDetector.clearCounter();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASH))) {
        switch (args.toLowerChar(0)) {
        case 'c': {
            bool cleared = false;
            if (args.has(F("shrink"))) {
                if (SaveCrash::clearStorage(SaveCrash::ClearStorageType::SHRINK)) {
                    cleared = true;
                }
                else {
                    args.print(F("SHRINK is currently not supported"));
                }
            }
            else if (args.has(F("version"))) {
                if (SaveCrash::clearStorage(SaveCrash::ClearStorageType::REMOVE_PREVIOUS_VERSIONS)) {
                    cleared = true;
                }
                else {
                    args.print(F("REMOVE_PREVIOUS_VERSIONS is currently not supported"));
                }
            }
            else {
                if (SaveCrash::clearStorage(SaveCrash::ClearStorageType::REMOVE_MAGIC)) {
                    cleared = true;
                }
            }
            if (cleared) {
                args.print(F("SaveCrash log have been cleared"));
            }
            else {
                args.print(F("while clearing SaveCrash logs, one or more errors occured"));
            }

        } break;
        case 'f': {
            if (SaveCrash::clearStorage(SaveCrash::ClearStorageType::ERASE)) {
                args.print(F("SaveCrash logs erased"));
            }
            else {
                args.print(F("erase SaveCrash logs reported one or more errors"));
            }
        } break;
        case 'i': {
            auto info = SaveCrash::createFlashStorage().getInfo();
            args.printf_P(PSTR("counter=%u size=%u sectors used=%u total=%u"), info._counter, info._size, info._sector_used, info._sectors_total);
            args.printf_P(PSTR("free space=%u largest block=%u"), info._space, info._spaceLargestBlock);
        } break;
        case 'l': {
            int16_t count = 0;
            PrintString timeStr;
            Stream &output = args.getStream();
            auto fs = SaveCrash::createFlashStorage();
            fs.getCrashLog([&](const SaveCrash::CrashLogEntry &item) {
                time_t now = static_cast<time_t>(item._header._time);
                timeStr.clear();
                timeStr.strftime(FSPGM(strftime_date_time_zone), localtime(&now));

                output.printf_P(PSTR("<%03u> "), ++count);
                output.print(timeStr);
                fs.printCrashLogEntry(output, item);
                return true;
            });
        } break;
        case 'p': {
            auto fs = SaveCrash::createFlashStorage();
            auto counter = fs.getInfo()._counter;
            if (counter == 0) {
                args.print(F("SaveCrash log is empty"));
            }
            else {
                auto number = args.toIntMinMax<int16_t>(1, 0, std::numeric_limits<int16_t>::max(), 0);
                if (number > 0 && number <= counter) {
                    int16_t count = 0;
                    fs.getCrashLog([&](const SaveCrash::CrashLogEntry &item) {
                        if (++count == number) {
                            fs.printCrashLog(args.getStream(), item);
                            return false;
                        }
                        return true;
                    });
                }
                else {
                    args.invalidArgument(1, reinterpret_cast<const __FlashStringHelper *>(PrintString(F("%u-%u"), 1, counter).c_str()));
                }
            }
        } break;
        default:
            args.invalidArgument(0, F(LIST_SAVE_CASH_COMMANDS), '|');
            break;
        }
        return true;
    }
    return false;
}

#endif
