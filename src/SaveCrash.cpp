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
                            WebUIAlerts_warning(PrintString(F("Crash dump saved to: %s"), filename.c_str()));
                        }
                    }
                }
            }
        });
    }

}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SAVECRASH, "SAVECRASH", "<clear|print|alloc>[,<alloc size|print stream>]", "Manage EspSaveCrash");

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
        __LDBG_printf("path=%s name=%s size=%u counter=%u espcounter=%u", path.c_str(), name.c_str(), size, counter, espSaveCrash.count());
        output.printf_P(PSTR("%u crash report(s), total size "), counter);
        output.print(formatBytes(size));
    }

public:
#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override {
        auto name = getName_P();
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASH), name);
    }

    virtual bool atModeHandler(AtModeArgs &args) override {
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASH))) {
            if (args.toLowerChar(0) == 'c') {
                espSaveCrash.clear();
                args.print(F("EEPROM cleared"));
            }
            else if (args.toLowerChar(0) == 'p' && args.requireArgs(1, 2)) {
                //TODO add stream name args.toString(1)
                // multiStream::create(args.toString(1))
                // return Serial0/1, create a file, append to file, open tcp connection
                espSaveCrash.print(args.getStream());
            }
            else if (args.toLowerChar(0) == 'a' && args.requireArgs(2, 2)) {
#if SAVE_CRASH_HAVE_CALLBACKS
                auto size = args.toIntMinMax(1U, 16U, 5000U);
                auto ptr = malloc(size);
                args.printf_P(PSTR("reserved %u byte (%p) for EspSaveCrash"), size, ptr);
                if (ptr) {
                    auto nextCallback = EspSaveCrash::getCallback();
                    EspSaveCrash::setCallback([nextCallback, ptr](const EspSaveCrash::ResetInfo_t &info) {
                        free(ptr);
                        if (nextCallback) {
                            nextCallback(info);
                        }
                    });
                }
#else
                args.print(F("EspSaveCrash callbacks not available"));
#endif
            }
            else {
                args.print(F("invalid argument"));
            }
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
