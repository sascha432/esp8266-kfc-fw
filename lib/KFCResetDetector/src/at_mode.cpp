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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(RD, "RD", "Reset detector clear counter", "Display information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SAVECRASH, "SAVECRASH", "<clear|list|print>[,<number>]", "Manage SaveCrash");

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
+SAVECRASH=print,0

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
                resetDetector.getResetReason()
            );
        }
        else {
            resetDetector.clearCounter();
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASH))) {
        if (args.toLowerChar(0) == 'c') {
            SaveCrash::clearEEPROM();
            args.print(F("EEPROM cleared"));
        }
        else if (args.toLowerChar(0) == 'i') {
            auto info = SaveCrash::createFlashStorage().getInfo();
            args.printf_P(PSTR("counter=%u size=%u sectors used=%u total=%u"), info._counter, info._size, info._sector_used, info._sectors_total);
            args.printf_P(PSTR("free space=%u largest block=%u"), info._space, info._spaceLargestBlock);
        }
        else if (args.toLowerChar(0) == 'l') {
            int16_t count = 0;
            PrintString timeStr;
            SaveCrash::createFlashStorage().getCrashLog([&](const SaveCrash::CrashLogEntry &item) {
                time_t now = static_cast<time_t>(item._header._time);
                timeStr.clear();
                timeStr.strftime(FSPGM(strftime_date_time_zone), localtime(&now));

                auto &stream = args.getStream();
                stream.printf_P(PSTR("<%03u> "), ++count);
                stream.print(timeStr);
                stream.print(F(" reason="));
                stream.print(item._header.getReason());
                stream.printf_P(PSTR(" excvaddr=0x%08x depc=0x%08x\n"), item._header._info.excvaddr, item._header._info.depc);
            });
        }
        else if (args.toLowerChar(0) == 'p') {
            auto number = args.toIntMinMax<int16_t>(1, 0, std::numeric_limits<int16_t>::max(), 0);
            int16_t count = 0;
            auto fs = SaveCrash::createFlashStorage();
            fs.getCrashLog([&](const SaveCrash::CrashLogEntry &item) {
                if (++count == number) {
                    fs.printCrashLog(args.getStream(), item);
                    number = -1;
                }
            });
            if (number != -1) {
                args.printf_P(PSTR("Crash log #%u not found"), number);
            }
        }
        else {
            args.print(F("invalid argument"));
        }
        return true;
    }
    return false;
}

#endif
