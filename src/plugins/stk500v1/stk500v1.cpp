/**
 * Author: sascha_lammers@gmx.de
 */

#include "STK500v1Programmer.h"
#include "at_mode.h"
#include "plugins.h"
#include <EventScheduler.h>
#if STK500_HAVE_SOFTWARE_SERIAL
#    include <SoftwareSerial.h>
#endif

#if DEBUG_STK500V1
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// Plugin

class STK500v1Plugin : public PluginComponent {
public:
    STK500v1Plugin();

    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(STK500V1F, "STK500V1F", "<filename>,[<0=Serial/1=Serial1>[,<0=disable/1=logger/2=serial/3=serial2http/4=file>]]", "Flash ATmega micro controller over Serial Port");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(STK500V1S, "STK500V1S", "<atmega328p/0x1e1234/...>", "Set signature (/stk500v1/atmega.csv)", "Display signature");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr STK500v1Plugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(STK500V1F),
        PROGMEM_AT_MODE_HELP_COMMAND(STK500V1S)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

class HardwareSerialHelper : public HardwareSerial {
public:
    // returns 0 if not active
    uint32_t getBaudRate() {
        return uart_get_baudrate(_uart);
    }
};

bool STK500v1Plugin::atModeHandler(AtModeArgs &args) {

    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1S))) {
        if (args.size() >= 1 && !STK500v1Programmer::getSignature(args.get(0), _signature)) {
            args.print(F("Name unknown"));
        }
        args.printf_P(PSTR("Signature set: %02x %02x %02x"), _signature[0], _signature[1], _signature[2]);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STK500V1F))) {
        if (args.requireArgs(1, 3)) {
            if (stk500v1) {
                args.print(F("In progress"));
            }
            else {
                uint32_t prevBaudRate = 0;
                PGM_P portName;
                Stream *serialPort;
                String filename = args.get(0);
                uint8_t serialPortNum = args.toInt(1);
                switch(serialPortNum) {
                    #if STK500_HAVE_SOFTWARE_SERIAL
                        case 2:
                            serialPort = new SoftwareSerial(STK500_HAVE_SOFTWARE_SERIAL_PINS);
                            reinterpret_cast<SoftwareSerial *>(serialPort)->begin(KFC_SERIAL_RATE);
                            portName = PSTR("SoftwareSerial");
                            break;
                        #endif
                    case 1:
                        prevBaudRate = reinterpret_cast<HardwareSerialHelper &>(Serial1).getBaudRate();
                        // Serial1.begin(KFC_SERIAL_RATE);
                        Serial1.setRxBufferSize(512);
                        serialPort = &Serial1;
                        portName = PSTR("Serial1");
                        break;
                    case 0:
                    default:
                        prevBaudRate = reinterpret_cast<HardwareSerialHelper &>(Serial0).getBaudRate();
                        // Serial0.begin(KFC_SERIAL_RATE);
                        Serial0.setRxBufferSize(512);
                        serialPort = &Serial0;
                        portName = PSTR("Serial");
                        break;
                }

                args.printf_P(PSTR("Flashing %s on %s\n"), filename.c_str(), portName);
                args.getStream().flush();

                stk500v1 = new STK500v1Programmer(*serialPort);
                stk500v1->setSignature(_signature);
                stk500v1->setFile(filename);
                stk500v1->setLogging(args.toInt(2, STK500v1Programmer::LOG_FILE));

                // run in main loop
                _Scheduler.add(1000, false, [this, serialPort, serialPortNum, prevBaudRate](Event::CallbackTimerPtr timer) {
                    stk500v1->begin([serialPort, serialPortNum, prevBaudRate]() {
                        switch(serialPortNum) {
                            #if STK500_HAVE_SOFTWARE_SERIAL
                                case 2:
                                    delete reinterpret_cast<SoftwareSerial *>(serialPort);
                                    break;
                                #endif
                            case 1:
                                Serial1.setRxBufferSize(256);
                                if (prevBaudRate) {
                                    Serial1.end();
                                    Serial1.begin(prevBaudRate);
                                }
                                break;
                            case 0:
                            default:
                                Serial0.setRxBufferSize(256);
                                if (prevBaudRate) {
                                    Serial0.end();
                                    Serial0.begin(prevBaudRate);
                                }
                                break;
                        }
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
