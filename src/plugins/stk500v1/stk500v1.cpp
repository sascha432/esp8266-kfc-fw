/**
 * Author: sascha_lammers@gmx.de
 */

#if STK500V1

#include <SoftwareSerial.h>
#include "stk500v1.h"
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
    virtual PGM_P getName() const;
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;

private:
    char _signature[3];
};

static STK500v1Plugin plugin;

STK500v1Plugin::STK500v1Plugin() {
    register_plugin(this);
}

PGM_P STK500v1Plugin::getName() const {
    return PSTR("stk500v1");
}

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(STK500V1F, "STK500V1F", "<filename>,[<port>[,<0=disable/1=logger/2=serial>]]", "Flash ATmega MCU");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(STK500V1S, "STK500V1S", "<atmega328p/328pb>", "Set signature", "Display signature");

bool STK500v1Plugin::hasAtMode() const {
    return true;
}

void STK500v1Plugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STK500V1F));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STK500V1S));
}

bool STK500v1Plugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(STK500V1S))) {
        if (argc != 1) {
            if (*_signature) {
                serial.printf_P(PSTR("+STK500V1S: Signature %02x %02x %02x\n"), _signature[0], _signature[1], _signature[2]);
            }
            else {
                serial.println(F("+STK500V1S: Default signature"));
            }
        }
        else {
            if (!STK500v1Programmer::getSignature(argv[0], _signature)) {
                serial.println(F("+STK500V1S: Unknown name"));
            }
            else {
                serial.println(F("+STK500V1S: Signature set"));
            }
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(STK500V1F))) {
        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else if (stk500v1) {
            serial.println(F("+STK500V1F: In progress"));
        }
        else {
            PGM_P portName;
            Stream *serialPort;
            String filename = argv[0];
            int port = argc >= 2 ? atoi(argv[1]) : 0;
            switch(port) {
                case 2:
                    serialPort = new SoftwareSerial(D5, D6);
                    reinterpret_cast<SoftwareSerial *>(serialPort)->begin(57600);
                    portName = PSTR("SoftwareSerial");
                    break;
                case 1:
                    serialPort = &Serial1;
                    portName = PSTR("Serial1");
                    break;
                case 0:
                default:
                    serialPort = &Serial;
                    portName = PSTR("Serial");
                    break;
            }

            serial.printf_P(PSTR("+STK500V1F: Flashing %s on %s\n"), filename.c_str(), portName);

            stk500v1 = new STK500v1Programmer(*serialPort);
            stk500v1->setSignature(_signature);
            stk500v1->setFile(filename);
            stk500v1->setLogging(argc >= 3 ? atoi(argv[2]) : STK500v1Programmer::LOG_DISABLED);
            stk500v1->begin([serialPort]() {
                if (serialPort != &Serial && serialPort != &Serial1) {
                    delete reinterpret_cast<SoftwareSerial *>(serialPort);
                }
                delete stk500v1;
                stk500v1 = nullptr;
            });
        }
        return true;
    }
    return false;
}

#endif
