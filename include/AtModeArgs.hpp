/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "at_mode.h"

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

inline AtModeArgs::ArgumentVector &AtModeArgs::getArgs()
{
    return _args;
}

inline const AtModeArgs::ArgumentVector &AtModeArgs::getArgs() const
{
    return _args;
}

inline AtModeArgs::ArgumentPtr AtModeArgs::operator[](int index) const
{
    if (index < size()) {
        return _args[index];
    }
    return nullptr;
}

inline uint16_t AtModeArgs::size() const
{
    return _args.size();
}

inline bool AtModeArgs::empty() const
{
    return _args.empty();
}

inline AtModeArgs::ArgumentVectorIterator AtModeArgs::begin()
{
    return _args.begin();
}

inline AtModeArgs::ArgumentVectorIterator AtModeArgs::end()
{
    return _args.end();
}

inline AtModeArgs::AtModeArgs(Stream &output) :
    _output(output),
    _queryMode(false)
{
}

inline AtModeArgs::AtModeArgs(const AtModeArgs &args) :
    _output(args._output),
    _command(args._command),
    _queryMode(args._queryMode)
{
}

inline void AtModeArgs::clear()
{
    _queryMode = false;
    _args.clear();
}

inline void AtModeArgs::setQueryMode(bool mode)
{
    if (mode) {
        _queryMode = true;
        _args.clear();
    }
    else {
        _queryMode = false;
    }
}

inline bool AtModeArgs::isQueryMode() const
{
    return _queryMode;
}

inline Stream &AtModeArgs::getStream() const
{
    return _output;
}

inline bool AtModeArgs::requireArgs(uint16_t min, uint16_t max) const
{
    if (_queryMode && min) {
        at_mode_print_invalid_arguments(_output, 0, min, max);
        return false;
    }
    if (_args.size() < min || _args.size() > max) {
        at_mode_print_invalid_arguments(_output, _args.size(), min, max);
        return false;
    }
    return true;
}

inline void AtModeArgs::setCommand(const char *command)
{
    _command = command;
    _command.toUpperCase();
}

inline String &AtModeArgs::getCommand()
{
    return _command;
}

inline const String &AtModeArgs::getCommand() const
{
    return _command;
}

inline bool AtModeArgs::isCommand(const __FlashStringHelper *command) const
{
    return _command.equalsIgnoreCase(command);
}

inline void AtModeArgs::printf_P(PGM_P format, ...) const
{
    va_list arg;
    va_start(arg, format);
    PrintString str(reinterpret_cast<const __FlashStringHelper *>(format), arg);
    va_end(arg);
    print(str.c_str());
}

inline void AtModeArgs::_printfLn(const __FlashStringHelper *format, ...) const
{
    va_list arg;
    va_start(arg, format);
    PrintString str(format, arg);
    va_end(arg);
    print(str.c_str());
}

inline void AtModeArgs::_println(const char *str) const
{
    print();
    _output.println(str);
}

inline void AtModeArgs::_println(const __FlashStringHelper *str) const
{
    print();
    _output.println(str);
}

inline void AtModeArgs::_println(const String &str) const
{
    print(str.c_str());
}

inline void AtModeArgs::print() const
{
    _output.printf_P(PSTR("+%s: "), _command.c_str());
}

inline void AtModeArgs::help() const
{
    _output.printf_P(PSTR("try +HELP=%s\n"), _command.c_str());
}

inline bool AtModeArgs::isInvalidArg(uint16_t num) const
{
    return num >= _args.size();
}

inline AtModeArgs::ArgumentPtr AtModeArgs::get(uint16_t num) const
{
    if (isInvalidArg(num)) {
        return nullptr;
    }
    return _args.at(num);
}

inline int AtModeArgs::toChar(uint16_t num, int defaultValue) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return *get(num);
}

inline int AtModeArgs::toLowerChar(uint16_t num, int defaultValue) const
{
    return tolower(toChar(num, defaultValue));
}

inline String AtModeArgs::toString(uint16_t num, const String &defaultStr) const
{
    if (isInvalidArg(num)) {
        return defaultStr;
    }
    return get(num);
}

inline int32_t AtModeArgs::toInt(uint16_t num, int32_t defaultValue, uint8_t base) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtol(get(num), nullptr, base);
}

inline uint32_t AtModeArgs::toInt(uint16_t num, uint32_t defaultValue, uint8_t base) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtoul(get(num), nullptr, base);
}

inline int64_t AtModeArgs::toInt(uint16_t num, int64_t defaultValue, uint8_t base) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtoll(get(num), nullptr, base);
}

inline uint64_t AtModeArgs::toInt(uint16_t num, uint64_t defaultValue, uint8_t base) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtoull(get(num), nullptr, base);
}

inline double AtModeArgs::toDouble(uint16_t num, double defaultValue) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtod(get(num), nullptr);
}

inline float AtModeArgs::toFloat(uint16_t num, float defaultValue) const
{
    if (isInvalidArg(num)) {
        return defaultValue;
    }
    return strtof(get(num), nullptr);
}

inline bool AtModeArgs::equalsIgnoreCase(uint16_t num, const __FlashStringHelper *str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return strcasecmp_P(get(num), RFPSTR(str)) == 0;
}

inline bool AtModeArgs::equalsIgnoreCase(uint16_t num, const String &str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return str.equalsIgnoreCase(get(num));
}

inline bool AtModeArgs::equals(uint16_t num, const __FlashStringHelper *str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return strcmp_P(get(num), RFPSTR(str)) == 0;
}

inline bool AtModeArgs::equals(uint16_t num, const String &str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return str.equals(get(num));
}

inline bool AtModeArgs::equals(uint16_t num, char ch) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    auto arg = get(num);
    return arg[0] == ch && arg[1] == 0;
}

inline bool AtModeArgs::startsWith(uint16_t num, const __FlashStringHelper *str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return strncmp_P(get(num), RFPSTR(str), strlen_P(RFPSTR(str))) == 0;
}

inline bool AtModeArgs::startsWithIgnoreCase(uint16_t num, const __FlashStringHelper *str) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return strncasecmp_P(get(num), RFPSTR(str), strlen_P(RFPSTR(str))) == 0;
}

inline bool AtModeArgs::isAnyMatchIgnoreCase(uint16_t num, const __FlashStringHelper *strings) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    return _isAnyMatchIgnoreCase(get(num), strings);
}

// return true for "", "*", "any", "all"
inline  bool AtModeArgs::isAny(uint16_t num) const
{
    if (isInvalidArg(num)) {
        return false;
    }
    if (_isAnyMatchIgnoreCase(get(num), F("|*|*.*|any|all"))) {
        return true;
    }
    return false;

}

inline bool AtModeArgs::isTrue(uint16_t num, bool bDefault) const
{
    ArgumentPtr arg;
    if (nullptr == (arg = get(num)) || *arg == 0) {
        return bDefault;
    }
    int result = 0;
    if ((_isValidInt(arg, result) && result != 0) || (_isAnyMatchIgnoreCase(arg, F("start|yes|y|true|on|enable|en|open")))) {
        return true;
    }
    return false;
}

inline bool AtModeArgs::isFalse(uint16_t num, bool bDefault) const
{
    if (isInvalidArg(num)) {
        return bDefault;
    }
    int result;
    auto arg = get(num);
    if ((_isValidInt(arg, result) && result == 0) || (_isAnyMatchIgnoreCase(arg, F("|stop|no|n|false|off|disable|dis|null|close|closed")))) {
        return true;
    }
    return false;
}


inline bool AtModeArgs::_isAnyMatchIgnoreCase(String str, const __FlashStringHelper *strings) const
{
    str.trim();
    return (stringlist_ifind_P(strings, str.c_str(), '|') != -1);
}

inline Range AtModeArgs::toRange(uint16_t num, uint32_t min, uint32_t max, const String &defaultValue)
{
    uint32_t from = 0;
    uint32_t to = ~0U;
    auto arg = toString(num, defaultValue);
    char *end = nullptr;
    from = strtoul(arg.c_str(), &end, 0);
    if (end) {
        while(isspace(*end)) {
            end++;
        }
        if (*end == ',' || *end == '-') {
            to = strtoul(end + 1, nullptr, 0);
            if (*end == ',') {
                to += from;
            }
        }
    }
    from = std::clamp(from, min, max);
    to = std::clamp(to, min + 1, max);
    return Range(from, to);
}

#if DEBUG_AT_MODE
#include <debug_helper_disable.h>
#endif
