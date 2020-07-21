/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <reset_detector.h>
#include <ListDir.h>
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
        removeCrashCounter();
    }

    void removeCrashCounter()
    {
        SPIFFS.begin();
        auto filename = String(FSPGM(crash_counter_file, "/.pvt/crash_counter"));
        if (SPIFFS.exists(filename)) {
            SPIFFS.remove(filename);
        }
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
                constexpr uint8_t max_limit = SAVECRASH_MAX_DUMPS;
                uint8_t num = 0;
                static_assert(SAVECRASH_MAX_DUMPS < std::numeric_limits<decltype(num)>::max(), "too many");
                PrintString filename;

                do {
                    filename = PrintString(FSPGM(crash_dump_file, "/.pvt/crash.%03x"), num++);
                } while(SPIFFS.exists(filename) && num < max_limit);

                if (num < max_limit) {
                    auto file = SPIFFS.open(filename, fs::FileOpenMode::write);
                    if (file) {
                        String hash;
                        KFCConfigurationClasses::System::Firmware::getElfHashHex(hash);
                        if (hash.length()) {
                            file.printf_P(PSTR("firmware.elf hash %s\n"), hash.c_str());
                        }
                        espSaveCrash.print(file);
                        if (file) {
                            file.close();
                            espSaveCrash.clear();
                            WebUIAlerts_add(PrintString(F("Crash dump saved to: %s"), filename.c_str()), AlertMessage::TypeEnum_t::WARNING);
                        }
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
    SaveCrashPlugin();

    virtual void setup(SetupModeType mode) {
        // no maintenance in safe mode
        if (mode != SetupModeType::SAFE_MODE) {
            // save and clear crash dump eeprom
            SaveCrash::installSafeCrashTimer(mode == SetupModeType::AUTO_WAKE_UP ? 60 : 10);
        }
    }

    virtual void getStatus(Print &output) override {
        int counter = espSaveCrash.count();
        String name = FSPGM(crash_dump_file);
        auto pos = name.indexOf('%');
        if (pos != -1) {
            name.remove(pos);
        }
        auto path = String('/');
        if ((pos = name.indexOf('/', 1)) != -1) {
            path = name.substring(0, pos);
        }
        size_t size = 0;
        ListDir dir(path);
        while(dir.next()) {
            if (dir.isFile() && dir.fileName().startsWith(name)) {
                counter++;
                size += dir.fileSize();
            }
        }
        debug_printf_P(PSTR("path=%s name=%s size=%u counter=%u espcounter=%u\n"), path.c_str(), name.c_str(), size, counter, espSaveCrash.count());
        output.printf_P(PSTR("%u crash report(s), total size "), counter);
        output.print(formatBytes(size));
    }

public:
#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override {
        auto name = getName_P();
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

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SaveCrashPlugin,
    "savecrash",        // name
    "SaveCrash",        // friendly name
    "",                 // web_templates
    "",                 // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::SAVECRASH,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    true,               // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

SaveCrashPlugin::SaveCrashPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SaveCrashPlugin))
{
    REGISTER_PLUGIN(this, "SaveCrashPlugin");
}

#endif
