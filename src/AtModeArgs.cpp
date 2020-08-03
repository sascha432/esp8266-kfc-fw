/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include "at_mode.h"
#include "kfc_fw_config.h"

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AtModeArgs::AtModeArgs(Stream &output) : _output(output) //, _argsArray(nullptr)
{
    clear();
}

// void AtModeArgs::_createArgs()
// {
//     uint16_t numItems = _args.size() + 1;
// #if AT_MODE_MAX_ARGUMENTS
//     if (AT_MODE_MAX_ARGUMENTS > numItems) {
//         numItems = AT_MODE_MAX_ARGUMENTS;
//     }
// #endif
//     uint8_t argc = 0;
//     _argsArray = (char **)calloc(numItems, sizeof(char *));
//     if (_argsArray) {
//         for(auto arg: _args) {
// #if 0
//             _argsArray[argc++] = strdup((const char *)arg);
// #else
//             _argsArray[argc++] = (char *)arg;
// #endif
//         }
//     }
// }

// void AtModeArgs::_freeArgs()
// {
//     if (_argsArray) {
// #if 0
//         auto ptr = _argsArray;
//         while(*ptr) {
//             free((void *)(*ptr));
//             ptr++;
//         }
// #endif
//         free(_argsArray);
//         _argsArray = nullptr;
//     }
// }

void AtModeArgs::clear()
{
    _queryMode = false;
    _args.clear();
    // _freeArgs();
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

long AtModeArgs::toInt(uint16_t num, long defaultValue) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        return atol(arg);
    }
    return defaultValue;
}

long AtModeArgs::toNumber(uint16_t num, long defaultValue) const
{
    ArgumentPtr arg;
    if (nullptr != (arg = get(num))) {
        char *endPtr = nullptr;
        auto value = strtol(arg, &endPtr, 0); // auto detect base
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
    auto arg = get(num);
    if (!arg) {
        _debug_printf_P(PSTR("toMillis(): arg=%u does not exist\n"), num);
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
    char suffixChar = suffix.length() == 1 ? suffix[0] : 0;
    if (suffixChar == 'm' || String_startsWith(suffix, SPGM(min, "min"))) {
        result =  (uint32_t)(value * 1000.0 * 60);
    }
    else if (suffixChar == 'h' || String_startsWith(suffix, SPGM(hour, "hour"))) {
        result =  (uint32_t)(value * 1000.0 * 60 * 60);
    }
    else if (suffixChar == 'd' || String_startsWith(suffix, SPGM(day, "day"))) {
        result =  (uint32_t)(value * 1000.0 * 60 * 60 * 24);
    }
    else if (suffixChar == 's' || String_startsWith(suffix, SPGM(sec, "sec"))) {
        result = (uint32_t)(value * 1000.0);
    }
    else {
        result = (uint32_t)value;   // use default suffix
    }
    result = std::min(maxTime, result);
    if (result < minTime) {
        _debug_printf_P(PSTR("toMillis(): arg=%s < minTime=%u\n"), arg, minTime);
        return defaultValue;
    }
    _debug_printf_P(PSTR("toMillis(): arg=%s converted to %u\n"), arg, result);
    return result;
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
        _output.println(FSPGM(OK, "OK"));
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
