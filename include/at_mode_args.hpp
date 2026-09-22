/**
  Author: sascha_lammers@gmx.de
*/

#if DEBUG_AT_MODE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
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
    __DBG_validatePointerCheck(command, VP_HPS);
    return _command.equalsIgnoreCase(command);
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

inline bool AtModeArgs::isInvalidArg(uint16_t num) const
{
    return num >= _args.size();
}

inline AtModeArgs::ArgumentPtr AtModeArgs::get(uint16_t num) const
{
    if (isInvalidArg(num)) {
        return emptyString.c_str();
    }
    return _args.at(num);
}

inline int AtModeArgs::toChar(uint16_t num, int defaultValue) const
{
    auto arg = get(num);
    if (!*arg) {
        return defaultValue;
    }
    return *arg;
}

inline int AtModeArgs::toLowerChar(uint16_t num, int defaultValue) const
{
    return tolower(toChar(num, defaultValue));
}

inline int32_t AtModeArgs::toInt(uint16_t num, int32_t defaultValue, uint8_t base) const
{
    return toInt(num, static_cast<int64_t>(defaultValue), base);
}

inline uint32_t AtModeArgs::toInt(uint16_t num, uint32_t defaultValue, uint8_t base) const
{
    return toInt(num, static_cast<uint64_t>(defaultValue), base);
}

inline uint64_t AtModeArgs::toInt(uint16_t num, uint64_t defaultValue, uint8_t base) const
{
    return toInt(num, static_cast<int64_t>(defaultValue), base); // strtoll and strtoull both support negative values
}

inline int64_t AtModeArgs::toInt(uint16_t num, int64_t defaultValue, uint8_t base) const
{
    auto arg = get(num);
    if (!*arg) {
        return defaultValue;
    }
    return strtoll(get(num), nullptr, base);
}

inline double AtModeArgs::toDouble(uint16_t num, double defaultValue) const
{
    auto arg = get(num);
    if (!*arg) {
        return defaultValue;
    }
    return strtod(get(num), nullptr);
}

inline float AtModeArgs::toFloat(uint16_t num, float defaultValue) const
{
    return toDouble(num, defaultValue);
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

inline bool AtModeArgs::_isAnyMatchIgnoreCase(String str, const __FlashStringHelper *strings) const
{
    str.trim();
    return (stringlist_ifind_P(strings, str.c_str(), '|') != -1);
}

#if DEBUG_AT_MODE
#    include <debug_helper_disable.h>
#endif
