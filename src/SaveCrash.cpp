/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <reset_detector.h>
#include <ListDir.h>
#include "progmem_data.h"
#include "SaveCrash.h"
#include "kfc_fw_config.h"
#include "WebUIAlerts.h"

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

    void installRemoveCrashCounter(uint32_t delay_seconds)
    {
        Scheduler.addTimer(delay_seconds * 1000UL, false, [](EventScheduler::TimerPtr timer) {
            removeCrashCounter();
            resetDetector.clearCounter();
        });
    }

}

#if DEBUG_HAVE_SAVECRASH

EspSaveCrash espSaveCrash;

namespace SaveCrash {

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
                        WebUIAlerts_add(PrintString(F("Crash dump saved to: %s"), filename.c_str()), AlertMessage::TypeEnum_t::WARNING);
                    }
                }
            }
        });
    }

}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHC, "SAVECRASHC", "Clear crash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHP, "SAVECRASHP", "Print saved crash details");

#endif

class SaveCrashPlugin : public PluginComponent {
// PluginComponent
public:
    SaveCrashPlugin() {
        REGISTER_PLUGIN(this);
    }

    virtual PGM_P getName() const {
        return PSTR("savecrash");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("EspSaveCrash");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return MAX_PRIORITY;
    }

#if SPIFFS_SUPPORT
    virtual void setup(PluginSetupMode_t mode) {
        // save and clear crash dump eeprom after
        SaveCrash::installSafeCrashTimer(10);
    }
#endif

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override {
        int counter = espSaveCrash.count();
#if SPIFFS_SUPPORT
        ListDir dir(String('/'));
        while(dir.next()) {
            if (dir.isFile() && dir.fileName().startsWith(F("/crash."))) {
                counter++;
            }
        }
#endif
        output.printf_P(PSTR("Crash reports: %u"), counter);
    }

public:
#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }

    virtual void atModeHelpGenerator() override {
        auto name = getName();
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHC), name);
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHP), name);
    }

    virtual bool atModeHandler(AtModeArgs &args) override {
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHC))) {
            espSaveCrash.clear();
            args.print(F("Cleared"));
            return true;
        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHP))) {
            espSaveCrash.print(args.getStream());
            return true;
        }
        return false;
    }

#endif
};

static SaveCrashPlugin plugin;

#endif
