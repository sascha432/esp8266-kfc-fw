/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if !AT_MODE_SUPPORTED

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF(name, str1, str2, str3, str4)      ;
#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(name, str3, str4)             ;
#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(name, str1, str2, str3)       ;
#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(name, str1, str3)             ;
#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(name, str1, str3, str4)       ;

#define PROGMEM_AT_MODE_HELP_COMMAND(name)                                  nullptr
#define PROGMEM_AT_MODE_HELP_COMMAND(name)                                nullptr

#else

#include <Arduino_compat.h>
#include <PrintString.h>
#include <algorithm>
#include <memory>
#include <vector>
#include "serial_handler.h"
#include <../include/time.h>

#ifndef DEBUG_AT_MODE
#define DEBUG_AT_MODE                           0
#endif

#ifndef AT_MODE_MAX_ARGUMENTS
#define AT_MODE_MAX_ARGUMENTS                   64
#endif

typedef struct ATModeCommandHelp_t {
    PGM_P command;
    PGM_P arguments;
    PGM_P help;
    PGM_P helpQueryMode;
    PGM_P commandPrefix;
} ATModeCommandHelp_t;

class ATModeCommandHelpData {
public:
    ATModeCommandHelpData(PGM_P command, PGM_P arguments, PGM_P help) : _data({command, arguments, help, nullptr, nullptr})  {}
    ATModeCommandHelpData(PGM_P command, PGM_P arguments, PGM_P help, PGM_P helpQueryMode) : _data({command, arguments, help, helpQueryMode, nullptr})  {}
    ATModeCommandHelpData(PGM_P command, PGM_P arguments, PGM_P help, PGM_P helpQueryMode, PGM_P commandPrefix) : _data({command, arguments, help, helpQueryMode, commandPrefix})  {}
    const struct ATModeCommandHelp_t _data;
};

class ATModeCommandHelp {
public:
    ATModeCommandHelp(const ATModeCommandHelp_t *data, PGM_P pluginName = nullptr);
    ATModeCommandHelp(const ATModeCommandHelp_t *data, const __FlashStringHelper *pluginName = nullptr) : ATModeCommandHelp(data, RFPSTR(pluginName)) {}

    PGM_P command() const;
    const __FlashStringHelper *getFPCommand() const {
        return FPSTR(command());
    }
    PGM_P commandPrefix() const;
    const __FlashStringHelper *getFPCommandPrefix() const {
        return FPSTR(commandPrefix());
    }
    PGM_P arguments() const;
    PGM_P help() const;
    PGM_P helpQueryMode() const;
    PGM_P pluginName() const;

    void setPluginName(const __FlashStringHelper *name);
    void setPluginName(PGM_P name);
private:
    const ATModeCommandHelp_t *_data;
    PGM_P _name;
};

#define PROGMEM_AT_MODE_HELP_COMMAND(name)          &_at_mode_progmem_command_help_t_##name
#define PROGMEM_AT_MODE_HELP_ARGS(name)             _at_mode_progmem_command_help_arguments_##name##[1]

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX         ""

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF(name, command, arguments, help, qhelp) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { command }; \
    static const char _at_mode_progmem_command_help_arguments_##name[] PROGMEM = { arguments }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { help }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { qhelp }; \
    static const char _at_mode_progmem_command_help_prefix_##name[] PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND_PREFIX }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        _at_mode_progmem_command_help_arguments_##name, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name, \
        (_at_mode_progmem_command_help_prefix_##name) \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(name, help, qhelp) \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { help }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { qhelp }; \
    static const char _at_mode_progmem_command_help_prefix_##name[] PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND_PREFIX }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        nullptr, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name, \
        (_at_mode_progmem_command_help_prefix_##name) \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(name, command, arguments, help) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { command }; \
    static const char _at_mode_progmem_command_help_arguments_##name[] PROGMEM = { arguments }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { help }; \
    static const char _at_mode_progmem_command_help_prefix_##name[] PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND_PREFIX }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        _at_mode_progmem_command_help_arguments_##name, \
        _at_mode_progmem_command_help_help_##name, \
        nullptr, \
        (_at_mode_progmem_command_help_prefix_##name) \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(name, command, help) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { command }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { help }; \
    static const char _at_mode_progmem_command_help_prefix_##name[] PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND_PREFIX }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        nullptr, \
        (_at_mode_progmem_command_help_prefix_##name) \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(name, command, help, qhelp) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { command }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { help }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { qhelp }; \
    static const char _at_mode_progmem_command_help_prefix_##name[] PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND_PREFIX }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name, \
        (_at_mode_progmem_command_help_prefix_##name) \
    };

void at_mode_setup();
void at_mode_add_help(const ATModeCommandHelp *help);
void at_mode_add_help(const ATModeCommandHelp_t *help, PGM_P pluginName);
void serial_handle_event(String command);
String at_mode_print_command_string(Stream &output, char separator);
void at_mode_serial_input_handler(Stream &client);
void at_mode_print_invalid_arguments(Stream &output, uint16_t num = 0, uint16_t min = ~0, uint16_t max = ~0);
void at_mode_print_prefix(Stream &output, const __FlashStringHelper *command);
void at_mode_print_prefix(Stream &output, const char *command);
inline void at_mode_print_prefix(Stream &output, const String &command) {
    at_mode_print_prefix(output, (const char *)command.c_str());
}
void enable_at_mode(Stream &output);
void disable_at_mode(Stream &output);

using AtModeResolveACallback = std::function<void(const String &name)>;

class AtModeArgs : public TokenizerArgs {
public:
    // TokenizerArgs implementation

    virtual void add(char *arg) {
        _args.push_back(arg);
    }
    virtual bool hasSpace() const {
        return true;
    }

public:
    using ArgumentPtr = const char *;
    using ArgumentVector = std::vector<ArgumentPtr>;
    using ArgumentVectorIterator = ArgumentVector::iterator;

    static constexpr uint32_t kNoDefaultValue = ~0;

public:
    AtModeArgs() = delete;
    // AtModeArgs(const AtModeArgs &) = delete;
    AtModeArgs(AtModeArgs &&) = delete;

    AtModeArgs(Stream &output);

    // does not copy the arguments vector _args
    AtModeArgs(const AtModeArgs &args);

    void clear();
    void setQueryMode(bool mode);
    bool isQueryMode() const;
    Stream &getStream() const;

    // arguments access
    ArgumentVector &getArgs();
    const ArgumentVector &getArgs() const;
    ArgumentPtr operator[](int index) const;
    uint16_t size() const;
    bool empty() const;
    ArgumentVectorIterator begin();
    ArgumentVectorIterator end();

    // returns true if argument does not exists
    bool isInvalidArg(uint16_t num) const;

    ArgumentPtr get(uint16_t num) const;

    // toInt() base is 10 by default
    // type is determined by defaultValue
    int32_t toInt(uint16_t num, int32_t defaultValue, uint8_t base = 10) const;
    uint32_t toInt(uint16_t num, uint32_t defaultValue, uint8_t base = 10) const;
    int64_t toInt(uint16_t num, int64_t defaultValue, uint8_t base = 10) const;
    uint64_t toInt(uint16_t num, uint64_t defaultValue, uint8_t base = 10) const;

    int32_t toInt(uint16_t num) const {
        return toInt(num, 0);
    }

    // toInt with type
    inline int8_t toInt8(uint16_t num, int8_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, static_cast<int32_t>(defaultValue), base);
    }
    inline uint8_t toUint8(uint16_t num, uint8_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, static_cast<uint32_t>(defaultValue), base);
    }
    inline int8_t toInt16(uint16_t num, int16_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, static_cast<int32_t>(defaultValue), base);
    }
    inline uint16_t toUint16(uint16_t num, uint16_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, static_cast<uint32_t>(defaultValue), base);
    }
    inline int32_t toInt32(uint16_t num, int32_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, defaultValue, base);
    }
    inline uint32_t toUint32(uint16_t num, uint32_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, defaultValue, base);
    }
    inline int64_t toInt64(uint16_t num, int64_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, defaultValue, base);
    }
    inline uint64_t toUint64(uint16_t num, uint64_t defaultValue = 0, uint8_t base = 10) const {
        return toInt(num, defaultValue, base);
    }

    double toDouble(uint16_t num, double defaultValue = 0) const;
    float toFloat(uint16_t num, float defaultValue = 0) const;

    // template<typename _Ta>
    // _Ta toIntT(uint16_t num, _Ta defaultValue = 0, uint8_t base = 10) const {
    //     return toInt(num, defaultValue, base);
    // }

    // toNumber() base is auto detect by default
    template<typename _Ta>
    _Ta toNumber(uint16_t num, _Ta defaultValue = 0, uint8_t base = 0) const {
        return toInt(num, defaultValue, base);
    }

    template<class _Ta>
    _Ta toIntMinMax(uint16_t num, _Ta min, _Ta max, _Ta defaultValue) const {
        if (isInvalidArg(num)) {
            return defaultValue;
        }
        return std::clamp<_Ta>(toInt(num, static_cast<_Ta>(0)), min, max);
    }

    template<class _Ta>
    _Ta toIntMinMax(uint16_t num, _Ta min, _Ta max) const {
        return toIntMinMax(num, min, max, min);
    }

    template<class _Ta>
    _Ta toFloatMinMax(uint16_t num, _Ta min, _Ta max, _Ta defaultValue) const {
        if (isInvalidArg(num)) {
            return defaultValue;
        }
        // return std::max(min, std::min(max, static_cast<_Ta>(toDouble(num))));
        return std::clamp<_Ta>(toDouble(num, defaultValue), min, max);
    }

    template<class _Ta>
    _Ta toFloatMinMax(uint16_t num, _Ta min, _Ta max) const {
        return toFloatMinMax(num, min, max, min);
    }

    // convert time to milliseconds
    // format:
    // <time as int or float><suffix[default=ms]>
    //
    // suffix:
    // ms, milli* - milliseconds
    // s, sec* - seconds
    // m, min* - minutes
    // h, hour* - hours
    // d, day* - days
    //
    // example:
    // "123ms" = 123
    // "15" = 15 (using default suffix)
    // "2.5s" = 2500
    // "10.7ms" = 10
    // "123s" = 123000
    // "7 days" = 604800000
    // "0.7days" = 60480000
    //
    // returns defaultValue if the time is lower than minTime or if the argument does not exist
    uint32_t toMillis(uint16_t num, uint32_t minTime = 0, uint32_t maxTime = ~0, uint32_t defaultValue = kNoDefaultValue, const String &defaultSuffix = String()) const;

    int toChar(uint16_t num, int defaultValue = -1) const;

    int toLowerChar(uint16_t num, int defaultValue = -1) const;

    String toString(uint16_t num, const String &defaultStr = String()) const;

    bool has(const __FlashStringHelper *str, bool ignoreCase = true) const;

    bool equalsIgnoreCase(uint16_t num, const __FlashStringHelper *str) const;
    bool equalsIgnoreCase(uint16_t num, const String &str) const;
    bool equals(uint16_t num, const __FlashStringHelper *str) const;
    bool equals(uint16_t num, const String &str) const;
    bool equals(uint16_t num, char ch) const;
    bool startsWith(uint16_t num, const __FlashStringHelper *str) const;
    bool startsWithIgnoreCase(uint16_t num, const __FlashStringHelper *str) const;
    bool isAnyMatchIgnoreCase(uint16_t num, const __FlashStringHelper *strings) const;

    // return true for "", "*", "any", "all"
    bool isAny(uint16_t num) const;

    // return true for "yes", "Y", "true", "start", "on", "enable", "en", "open" and any integer != 0
    // missing argument returns bDefault
    bool isTrue(uint16_t num, bool bDefault = false) const;
    // return true for "", "stop", "no", "N", "false", "off", "disable", "dis", "close", "closed" and any integer == 0
    // missing argument returns bDefault
    bool isFalse(uint16_t num, bool bDefault = false) const;

    struct Range {
        uint32_t offset;
        uint32_t size;
        Range() : offset(0), size(~0U) {
        }
        Range(uint32_t _offset) : offset(_offset), size(0U - _offset) {
        }
        Range(uint32_t _offset, uint32_t _size) : offset(_offset), size(_size) {
        }
    };

    // get range from min-max
    // syntax: <min>[-<max|min+1>] or <offset>[,<length|1>]
    Range toRange(uint16_t num, uint32_t min, uint32_t max, const String &defaultValue);

public:
    bool requireArgs(uint16_t min, uint16_t max = ~0) const;

public:
    void setCommand(const char *command);
    String &getCommand();
    const String &getCommand() const;
    bool isCommand(const ATModeCommandHelp_t *help) const;
    bool isCommand(const __FlashStringHelper *command) const;

    // deprecated
    void printf_P(PGM_P format, ...) const;

    // print(const __FlashStringHelper *fmt, ...) printf_P(fmt, ...) + "\n"
    template<typename _Ta, typename ... _Args>
    void print(_Ta arg, _Args ...args) const {
        _printfLn((const __FlashStringHelper *)arg, args...);
    }

    // print(str) str + "\n"
    template<typename _Ta>
    void print(_Ta arg) const {
        _println(arg);
    }

    // "+CMD: ""
    void print() const;
    // "OK - SAFE MODE"
    void ok() const;
    // "try +HELP=COMMAND\n"
    void help() const;
    // makeList = '|' converts expected from "a|b|c" to "[a, b, c]"
    void invalidArgument(uint16_t num, const __FlashStringHelper *expected = nullptr, char makeList = 0) const;

private:
    void _printfLn(const __FlashStringHelper *, ...) const;
    void _println(const __FlashStringHelper *str) const;
    void _println(const char *str) const;
    void _println(const String &str) const;

    template <class _Ta>
    bool _isValidInt(const char *str, _Ta &result) const {
        char *endPtr = nullptr;
        auto value = static_cast<_Ta>(strtoll(str, &endPtr, 10));
        // no end pointer?
        if (!endPtr) {
            return false;
        }
        // strip trailing spaces
        while(isspace(*endPtr)) {
            endPtr++;
        }
        // end of string, return value
        if (*endPtr == 0) {
            result = value;
            return true;
        }
        // some more text or numbers
        return false;
    }

    bool _isAnyMatchIgnoreCase(String str, const __FlashStringHelper *strings) const;

private:
    Stream &_output;
    String _command;
    ArgumentVector _args;
    bool _queryMode;
};

#include "AtModeArgs.hpp"

#endif
