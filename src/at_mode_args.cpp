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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

void AtModeArgs::ok() const
{
    if (config.isSafeMode()) {
        _output.println(F("OK - SAFE MODE"));
    }
    else {
        _output.println(F("OK"));
    }
}

void AtModeArgs::help() const
{
    _output.print(F("try https://github.com/sascha432/esp8266-kfc-fw/blob/master/docs/AtModeHelp.md#"));
    _output.println(_command);
}

// the following implementations are too big to be inlined and have been moved out of at_mode_args.hpp

bool AtModeArgs::requireArgs(uint16_t min, uint16_t max) const
{
    if (_queryMode && min) {
        ATMode::printInvalidArguments(_output, 0, min, max);
        return false;
    }
    if (_args.size() < min || _args.size() > max) {
        ATMode::printInvalidArguments(_output, _args.size(), min, max);
        return false;
    }
    return true;
}

String AtModeArgs::toString(uint16_t num, const String &defaultStr) const
{
    auto arg = get(num);
    if (!*arg) {
        return defaultStr;
    }
    return arg;
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

AtModeArgs::Range AtModeArgs::toRange(uint16_t num, uint32_t min, uint32_t max, const String &defaultValue)
{
    uint32_t from = 0;
    uint32_t to = ~0U;
    auto arg = get(num);
    if (!*arg) {
        arg = defaultValue.c_str();
    }
    char *end = nullptr;
    from = strtoul(arg, &end, 0);
    if (end) {
        while(isspace(*end)) {
            end++;
        }
        if (*end == ',' || *end == '-') {
            to = strtoul(end + 1, nullptr, 0);
            if (*end == ',') {
                to = from + std::max(1U, to) - 1;
            }
        }
    }
    from = std::clamp(from, min, max);
    to = std::clamp(to, min, max);
    if (from > to) {
        std::swap(from, to);
    }
    return Range(from, to);
}

uint32_t AtModeArgs::toMillis(uint16_t num, uint32_t minTime, uint32_t maxTime, uint32_t defaultValue, const String &defaultSuffix) const
{
    if (defaultValue == kNoDefaultValue) {
        defaultValue = minTime;
    }
    auto arg = get(num);
    if (!*arg) {
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
    if (result < minTime || result > maxTime) {
        return defaultValue;
    }
    return result;
}

void AtModeArgs::invalidArgument(uint16_t num, const __FlashStringHelper *expected, char makeList) const
{
    print();
    auto arg = get(num);
    if (isInvalidArg(num++)) {
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

// return true for "", "*", "any", "all"
bool AtModeArgs::isAny(uint16_t num) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    if (_isAnyMatchIgnoreCase(get(num), F("|*|*.*|any|all"))) {
        return true;
    }
    return false;

}

bool AtModeArgs::isTrue(uint16_t num, bool bDefault) const
{
    if (isInvalidArg(num)) {
        return bDefault;
    }
    auto arg = get(num);
    if (_isAnyMatchIgnoreCase(arg, F("start|yes|y|true|on|enable|en|open"))) { // match string
        return true;
    }
    int result;
    if (_isValidInt(arg, result)) { // match integer
        return result != 0;
    }
    return bDefault;
}

bool AtModeArgs::isFalse(uint16_t num, bool bDefault) const
{
    if (isInvalidArg(num)) {
        return bDefault;
    }
    auto arg = get(num);
    if (_isAnyMatchIgnoreCase(arg, F("|stop|no|n|false|off|disable|dis|null|close|closed"))) { // match string
        return true;
    }
    int result;
    if (_isValidInt(get(num), result)) { // match integer
        return result == 0;
    }
    return bDefault;
}

#endif
