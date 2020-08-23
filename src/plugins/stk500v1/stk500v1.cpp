/**
 * Author: sascha_lammers@gmx.de
 */

#include "stk500v1.h"
// SoftwareSerial removed, does not compile, type error
//#include <SoftwareSerial.h>
#include <EventScheduler.h>
#include "at_mode.h"
#include "plugins.h"
#include "STK500v1Programmer.h"

#if DEBUG_STK500V1
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// Plugin

class STK500v1Plugin : public PluginComponent {
public:
    STK500v1Plugin();

    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;

private:
    char _signature[3];
};

static STK500v1Plugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    STK500v1Plugin,
    "stk500v1",         // name
    "STK500v1",         // friendly name
    "",                 // web_templates
    "",                 // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::MIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

STK500v1Plugin::STK500v1Plugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(STK500v1Plugin))
{
    REGISTER_PLUGIN(this, "STK500v1Plugin");
}


PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(STK500V1F, "STK500V1F", "<filename>,[<0=Serial/1=Serial1>[,<0=disable/1=logger/2=serial/3=serial2http/4=file>]]", "Flash ATmega micro controller");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(STK500V1S, "STK500V1S", "<atmega328p/0x1e1234/...>", "Set signature (/stk500v1/atmega.csv)", "Display signature");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(STK500V1L, "STK500V1L", "Dump debug log file (/stk500v1/debug.log)");

void STK500v1Plugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1F), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1S), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1L), name);
}

bool STK500v1Plugin::atModeHandler(AtModeArgs &args) {

    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1S))) {
        if (args.size() >= 1 && !STK500v1Programmer::getSignature(args.get(0), _signature)) {
            args.print(F("Name unknown"));
        }
        args.printf_P(PSTR("Signature set: %02x %02x %02x"), _signature[0], _signature[1], _signature[2]);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1L))) {
        args.print(F("+STK500V1L: --- start ---"));
        STK500v1Programmer::dumpLog(args.getStream());
        args.print(F("+STK500V1L: --- end ---"));
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1F))) {
        if (args.requireArgs(1, 3)) {
            if (stk500v1) {
                args.print(F("In progress"));
            }
            else {
                PGM_P portName;
                Stream *serialPort;
                String filename = args.get(0);
                switch(args.toInt(1)) {
                    // case 2:
                    //     serialPort = new SoftwareSerial(D5, D6);
                    //     reinterpret_cast<SoftwareSerial *>(serialPort)->begin(57600);
                    //     portName = PSTR("SoftwareSerial");
                    //     break;
                    case 1:
                        Serial1.setRxBufferSize(512);
                        serialPort = &Serial1;
                        portName = PSTR("Serial1");
                        break;
                    case 0:
                    default:
                        Serial0.setRxBufferSize(512);
                        serialPort = &Serial0;
                        portName = PSTR("Serial");
                        break;
                }

                args.printf_P(PSTR("Flashing %s on %s\n"), filename.c_str(), portName);

                stk500v1 = new STK500v1Programmer(*serialPort);
                stk500v1->setSignature(_signature);
                stk500v1->setFile(filename);
                stk500v1->setLogging(args.toInt(2, STK500v1Programmer::LOG_FILE));

                // run in main loop
                _Scheduler.add(1000, false, [this, serialPort](Event::CallbackTimerPtr timer) {
                    stk500v1->begin([serialPort]() {
                        // if (serialPort != &Serial && serialPort != &Serial1) {
                        //     delete reinterpret_cast<SoftwareSerial *>(serialPort);
                        // }
                        delete stk500v1;
                        stk500v1 = nullptr;
                    });
                });
            }
        }
        return true;
    }
    return false;
}
