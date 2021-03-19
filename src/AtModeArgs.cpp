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

AtModeArgs::AtModeArgs(Stream &output) : _output(output)
{
    clear();
}

void AtModeArgs::clear()
{
    _queryMode = false;
    _args.clear();
}

void AtModeArgs::setQueryMode(bool mode)
{
    if (mode) {
        _queryMode = true;
        _args.clear();
    }
    else {
        _queryMode = false;
    }
}

bool AtModeArgs::isCommand(const ATModeCommandHelp_t *help) const
{
    size_t prefixLen;
    if (help->commandPrefix && ((prefixLen = strlen_P(help->commandPrefix)) != 0)) {
        if (!_command._startsWithIgnoreCase_P(help->commandPrefix, prefixLen, 0)) {
            __DBG_printf("cmd %s=%s _startsWithIgnoreCase_P=%u prefix=%s", _command.c_str() + prefixLen, help->command, _command._startsWithIgnoreCase_P(help->commandPrefix, prefixLen, 0), help->commandPrefix);
            return false;
        }
        __DBG_printf("cmd %s=%s equalsIgnoreCase=%u prefix=%s", _command.c_str() + prefixLen, help->command, _command.equalsIgnoreCase(FPSTR(help->command), prefixLen), help->commandPrefix);
        return _command.equalsIgnoreCase(FPSTR(help->command), prefixLen);
    }
    return _command.equalsIgnoreCase(FPSTR(help->command));
}

AtModeArgs::ArgumentPtr AtModeArgs::get(uint16_t num) const
{
    if (!exists(num)) {
        return nullptr;
    }
    return _args.at(num);
}

int AtModeArgs::toChar(uint16_t num, int defaultValue) const
{
    if (!exists(num)) {
        return defaultValue;
    }
    auto str = _args.at(num);
    return *str;
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

long AtModeArgs::toInt(uint16_t num, long defaultValue) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        return atol(arg);
    }
    return defaultValue;
}

long AtModeArgs::toNumber(uint16_t num, long defaultValue, uint8_t base) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        char *endPtr = nullptr;
        auto value = strtol(arg, &endPtr, base); // auto detect base
        if (!endPtr) {
            return defaultValue;
        }
        while(isspace(*endPtr)) {
            endPtr++;
        }
        if (*endPtr) { // characters after the integer
            return defaultValue;
        }
        return value;
    }
    return defaultValue;
}

long long AtModeArgs::toLongLong(uint16_t num, long long defaultValue) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        return atoll(arg);
    }
    return defaultValue;
}

double AtModeArgs::toDouble(uint16_t num, double defaultValue) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        return atof(arg);
    }
    return defaultValue;
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
    suffix.toLowerCase();

    uint32_t result;
    if (suffix == F("ms") || suffix.startsWith(F("mil"))) {
        result =  value;
    }
    else if (suffix.startsWith(F("s"))) {
        result =  value * 1000;
    }
    else if (suffix.startsWith(F("m"))) {
        result =  value * 1000 * 60;
    }
    else if (suffix.startsWith(F("h"))) {
        result =  value * 1000 * 3600;
    }
    else if (suffix.startsWith(F("d"))) {
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

void AtModeArgs::printf_P(PGM_P format, ...)
{
    va_list arg;
    va_start(arg, format);
    PrintString str(reinterpret_cast<const __FlashStringHelper *>(format), arg);
    va_end(arg);
    print(str.c_str());
}

void AtModeArgs::print(const char *str)
{
    print();
    _output.println(str);
}

void AtModeArgs::print(const __FlashStringHelper *str)
{
    print();
    _output.println(str);
}

void AtModeArgs::print()
{
    _output.printf_P(PSTR("+%s: "), _command.c_str());
}

void AtModeArgs::ok()
{
    if (config.isSafeMode()) {
        _output.println(F("OK - SAFE MODE"));
    }
    else {
        _output.println(FSPGM(OK));
    }
}

void AtModeArgs::help()
{
    _output.printf_P(PSTR("try +HELP=%s\n"), _command.c_str());
}

void AtModeArgs::invalidArgument(uint16_t num, const __FlashStringHelper *expected, char makeList)
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

#endif
