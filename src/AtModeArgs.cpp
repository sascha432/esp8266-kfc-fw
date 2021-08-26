/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <stl_ext/algorithm.h>
#include "at_mode.h"
#include "kfc_fw_config.h"
#include <Form.h>

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

bool AtModeArgs::isCommand(const ATModeCommandHelp_t *help) const
{
    __DBG_validatePointer(help, VP_HPS);
    __DBG_validatePointer(help->commandPrefix, VP_NHPS);
    __DBG_validatePointer(help->command, VP_HPS);
    if (help->commandPrefix && pgm_read_byte(help->commandPrefix)) { // check if not nullptr or empty string
        // check prefix and rest of the command
        return _command.startsWithIgnoreCase(FPSTR(help->commandPrefix)) && _command.equalsIgnoreCase(FPSTR(help->command), strlen_P(help->commandPrefix));
    }
    // compare commands
    return _command.equalsIgnoreCase(FPSTR(help->command));

}

bool AtModeArgs::has(const __FlashStringHelper *str, bool ignoreCase) const
{
    for (auto arg: _args) {
        if (ignoreCase) {
            if (strcasecmp_P(arg, reinterpret_cast<PGM_P>(str)) == 0) {
                return true;
            }
        }
        else if (strcmp_P(arg, reinterpret_cast<PGM_P>(str)) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t AtModeArgs::toMillis(uint16_t num, uint32_t minTime, uint32_t maxTime, uint32_t defaultValue, const String &defaultSuffix) const
{
    if (defaultValue == kNoDefaultValue) {
        defaultValue = minTime;
    }

    auto arg = get(num);
    if (!arg) {
        __LDBG_printf("toMillis(): arg=%u does not exist", num);
        if (defaultValue == kNoDefaultValue) {
            return std::clamp<uint32_t>(0, minTime, maxTime);
        }
        return defaultValue;
    }

    char *endPtr = nullptr;
    auto value = strtod(arg, &endPtr);
    String suffix(endPtr);
    suffix.trim();
    if (suffix.length() == 0) {
        suffix = defaultSuffix;
    }

    uint32_t result;
    if (suffix.startsWithIgnoreCase(F("ms")) || suffix.startsWithIgnoreCase(F("mil"))) {
        result =  value;
    }
    else if (suffix.startsWithIgnoreCase('s')) {
        result =  value * 1000;
    }
    else if (suffix.startsWithIgnoreCase('m')) {
        result =  value * 1000 * 60;
    }
    else if (suffix.startsWithIgnoreCase('h')) {
        result =  value * 1000 * 3600;
    }
    else if (suffix.startsWithIgnoreCase('d')) {
        result =  value * 1000 * 86400;
    }
    else {
        result = value;
    }
    if (result < minTime) {
        __LDBG_printf("toMillis(): arg=%s < minTime=%u", arg, minTime);
        result = defaultValue;
    }
    __LDBG_printf("toMillis(): arg=%s converted to %u", arg, result);
    return std::clamp(result, minTime, maxTime);
}

void AtModeArgs::invalidArgument(uint16_t num, const __FlashStringHelper *expected, char makeList) const
{
    print();
    auto arg = get(num++);
    if (arg) {
        _output.printf_P(PSTR("Invalid argument %u: %s"), num, arg);

    } else {
        _output.printf_P(PSTR("Argument %u missing"), num);
    }
    if (expected) {
        if (makeList) {
            String str = String('[');
            str += expected;
            str.replace(String(makeList), F(", "));
            str += ']';
        }
        _output.printf_P(PSTR(": expected: %s"), expected);
    }
    _output.println();
    help();
}

void AtModeArgs::ok() const
{
    if (config.isSafeMode()) {
        _output.println(F("OK - SAFE MODE"));
    }
    else {
        _output.println(FSPGM(OK));
    }
}

#endif
