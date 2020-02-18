/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntities.h>
#include "progmem_data.h"
#include "PinDebugger.h"

#if DEBUG_VIRTUAL_PIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if PIN_DEBUGGER

static PinDebuggerPlugin plugin;

void PinDebuggerPlugin::setup(PluginSetupMode_t mode)
{
}

void PinDebuggerPlugin::reconfigure(PGM_P source)
{
}

void PinDebuggerPlugin::restart()
{
    VirtualPinDebugger::removeAll();
}

void PinDebuggerPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Active pins %u" HTML_S(br)), VirtualPinDebugger::getPinCount());
    if (VirtualPinDebugger::getPinCount()) {
        auto &dbg = VirtualPinDebugger::getInstance();
        int n = 0;
        for(auto &pin: dbg.getPins()) {
            if (n++ != 0) {
                output.print(F(", "));
            }
            output.printf_P(PSTR("#%u=%u"), pin->getPin(), pin->analogRead());
            if (pin->getTimeout()) {
                output.printf_P(PSTR(" (timeout %u)"), pin->getTimeout());
            }
        }
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "PDBG"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PDBGLS, "LS", "Display debug pins");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGSET, "SET", "<pin>,<on/off>", "Configure pin for debugging");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGS, "S", "<pin>,<value=int|remove>", "Set or remove value that reading the PIN returns");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGSQ, "SQ", "<pin>,<value=int>,<timeout=ms>,[...]", "Set values in sequence");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGR, "R", "<pin>", "Read PIN value");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGW, "W", "<pin>,<value>,<analog|digital=default>", "Write value to PIN");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PDBGM, "M", "<pin>,<mode=in|out|int>", "Set mode for PIN");

void PinDebuggerPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGLS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGSET), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGSQ), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGR), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGW), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PDBGM), name);
}

static VirtualPinDebug &getPin(AtModeArgs &args, uint8_t pin, bool &exists)
{
    auto ptr = VirtualPinDebugger::getPin(pin);
    exists = ptr != nullptr;
    if (!exists) {
        args.print(F("PIN not found"));
    }
    return *ptr;
}

bool PinDebuggerPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGLS))) {
        if (VirtualPinDebugger::getPinCount()) {
            auto &serial = args.getStream();
            auto &dbg = VirtualPinDebugger::getInstance();
            int n = 0;
            args.print();
            serial.print(F("Pins: "));
            for(auto pin: dbg.getPins()) {
                if (n++ != 0) {
                    serial.print(F(", "));
                }
                serial.printf_P(PSTR("#%u"), pin->getPin());
                if (pin->hasReadValue()) {
                    serial.printf_P(PSTR(" readValue=%u"), pin->getReadValue());
                }
                if (pin->getTimeout()) {
                    serial.printf_P(PSTR(" timeout=%u"), pin->getTimeout());
                }
            }
            serial.println();
        }
        else {
            args.print(F("No active pins"));
        }

        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGSET))) {
        if (args.requireArgs(2, 2)) {
            auto pin = (uint8_t)args.toInt(0);
            bool state = args.isTrue(1);
            auto debugPin = VirtualPinDebugger::getPin(pin);
            if (debugPin && !state) {
                VirtualPinDebugger::removePin(debugPin);
            }
            else if (!debugPin && state) {
                VirtualPinDebugger::addPin(new VirtualPinDebug(args.toInt(0)));
            }
            args.printf_P(PSTR("PIN %u %s"), pin, state ? SPGM(enabled) : SPGM(disabled));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGR))) {
        if (args.requireArgs(1, 1)) {
            auto pin = (uint8_t)args.toInt(0);
            bool pinExists;
            auto &debugPin = getPin(args, pin, pinExists);
            if (pinExists) {
                if (debugPin.isAnalog()) {
                    args.printf_P(PSTR("PIN %u digitalRead: %u"), debugPin.getPin(), debugPin.digitalRead());
                }
                else {
                    args.printf_P(PSTR("PIN %u analogRead: %u"), debugPin.getPin(), debugPin.analogRead());
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGW))) {
        if (args.requireArgs(2, 3)) {
            auto pin = (uint8_t)args.toInt(0);
            auto value = args.toInt(1);
            bool analog = args.toLowerChar(2) == 'a';
            bool pinExists;
            auto &debugPin = getPin(args, pin, pinExists);
            if (pinExists) {
                if (analog) {
                    args.printf_P(PSTR("PIN %u analogWrite: %u"), debugPin.getPin(), value);
                    debugPin.analogWrite(value);
                }
                else {
                    args.printf_P(PSTR("PIN %u digitalWrite: %u"), debugPin.getPin(), value);
                    debugPin.digitalWrite(value);
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGS))) {
        if (args.requireArgs(2, 2)) {
            auto pin = (uint8_t)args.toInt(0);
            auto value = args.toInt(1);
            bool pinExists;
            auto &debugPin = getPin(args, pin, pinExists);
            if (pinExists) {
                if (args.equalsIgnoreCase(1, F("remove"))) {
                    args.printf_P(PSTR("PIN %u setReadValue removed"), debugPin.getPin());
                    debugPin.removeReadValue();
                }
                else {
                    args.printf_P(PSTR("PIN %u setReadValue: %u"), debugPin.getPin(), value);
                    debugPin.setReadValue(value);
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGSQ))) {
        if (args.requireArgs(5)) {
            auto pin = (uint8_t)args.toInt(0);
            bool pinExists;
            auto &debugPin = getPin(args, pin, pinExists);
            if (pinExists) {
                auto &serial = args.getStream();
                args.print();
                serial.printf_P(PSTR("PIN %u sequence: "), debugPin.getPin());
                VirtualPinDebug::SequenceVector sequence;
                for(uint16_t i = 1; i < args.size(); i += 2) {
                    auto value = args.toInt(i);
                    auto timeout = args.toIntMinMax<int>(i + 1, 5, 10000000, 5);
                    serial.printf_P(PSTR("%u (%ums) "), value, timeout);
                    sequence.emplace_back(value, timeout);
                }
                serial.println();
                debugPin.setReadValueSequence(sequence);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PDBGM))) {
        auto values = VirtualPinMode::getValues();
        // auto &serial = args.getStream();
        if (args.isQueryMode()) {
            args.printf_P("values=%s", VirtualPinMode::getValuesAsString(F("|")).c_str());
        }
        else  if (args.requireArgs(2, 2)) {
            auto pin = (uint8_t)args.toInt(0);
            auto modeStr = args.toString(1);
            StringVector results;
            auto mode = VirtualPinMode::parseString(modeStr, &results);
            args.printf_P(PSTR("mode=%08x from=modeStr parsed=%s"), mode, modeStr.c_str(), implode('|', results).c_str());
            pinMode(pin, mode);
        }
        return true;
    }
    return false;
}

#endif

#endif
