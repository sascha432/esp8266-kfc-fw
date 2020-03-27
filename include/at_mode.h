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
#define PROGMEM_AT_MODE_HELP_COMMAND_T(name)                                nullptr

#else

#include <Arduino_compat.h>
#include <PrintString.h>
#include <algorithm>
#include <memory>
#include <vector>

#ifndef DEBUG_AT_MODE
#define DEBUG_AT_MODE                           0
#endif

#ifndef AT_MODE_ALLOW_PLUS_WITHOUT_AT
#define AT_MODE_ALLOW_PLUS_WITHOUT_AT           1     // treat + and AT+ the same
#endif

#ifndef AT_MODE_MAX_ARGUMENTS
#define AT_MODE_MAX_ARGUMENTS                   16
#endif

typedef struct {
    PGM_P command;
    PGM_P arguments;
    PGM_P help;
    PGM_P helpQueryMode;
    PGM_P commandPrefix;
} ATModeCommandHelp_t;

class ATModeCommandHelp {
public:
    ATModeCommandHelp(const ATModeCommandHelp_t *data, PGM_P newPluginName) {
        command = data->command;
        commandPrefix = data->commandPrefix;
        arguments = data->arguments;
        help = data->help;
        helpQueryMode = data->helpQueryMode;
        pluginName = newPluginName;
    }

    PGM_P command;
    PGM_P commandPrefix;
    PGM_P arguments;
    PGM_P help;
    PGM_P helpQueryMode;
    PGM_P pluginName;
};

#define PROGMEM_AT_MODE_HELP_COMMAND(name)          &_at_mode_progmem_command_help_t_##name
#define PROGMEM_AT_MODE_HELP_COMMAND_T(name)        &_at_mode_progmem_command_help_t_##name

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
void at_mode_add_help(const ATModeCommandHelp_t *help, PGM_P pluginName);
void serial_handle_event(String command);
String at_mode_print_command_string(Stream &output, char separator);
void at_mode_serial_input_handler(uint8_t type, const uint8_t *buffer, size_t len);
void at_mode_print_invalid_arguments(Stream &output, uint16_t num = 0, uint16_t min = ~0, uint16_t max = ~0);
void at_mode_print_prefix(Stream &output, const __FlashStringHelper *command);
void at_mode_print_prefix(Stream &output, const char *command);
inline void at_mode_print_prefix(Stream &output, const String &command) {
    at_mode_print_prefix(output, (const char *)command.c_str());
}
typedef std::function<void(const String &name)> AtModeResolveACallback;
void at_mode_resolve(void *ptr, AtModeResolveACallback resolve_callback);
void enable_at_mode(Stream &output);
void disable_at_mode(Stream &output);

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
    typedef const char *ArgumentPtr;
    typedef std::vector<ArgumentPtr> ArgumentVector;
    typedef ArgumentVector::iterator ArgumentVectorIterator;

    typedef enum {
        ARG_ONE = 0,
        FIRST = 0,
        SECOND = 1,
        THIRD = 2,
        FOURTH = 3,
        FIFTH = 4,
        SIXTH = 5,
        SEVENTH = 6,
        EIGHTH = 7,
        NINTH = 8,
        TENTH = 9,
        ARG_TEN = 9,
    } ArgumentEnum_t;

public:
    AtModeArgs() = delete;
    // AtModeArgs(const AtModeArgs &) = delete;
    AtModeArgs(AtModeArgs &&) = delete;

    AtModeArgs(Stream &output);
    void clear();
    void setQueryMode(bool mode);

    AtModeArgs(const AtModeArgs &args) : _output(args._output) {
        _command = args._command;
        _args = args._args;
        _queryMode = args._queryMode;
    }

    Stream &getStream() {
        return _output;
    }

    bool isQueryMode() const {
        return _queryMode;
    }

    ArgumentVector &getArgs() {
        return _args;
    }

    inline uint16_t size() const {
        return _args.size();
    }

    inline bool empty() const {
        return _args.empty();
    }

    inline ArgumentVectorIterator begin() {
        return _args.begin();
    }

    inline ArgumentVectorIterator end() {
        return _args.end();
    }

    inline bool exists(uint16_t num) const {
        return num < _args.size();
    }

    ArgumentPtr get(uint16_t num) const;
    long toInt(uint16_t num, long defaultValue = 0) const;
    long toNumber(uint16_t num, long defaultValue = 0) const; // auto detect octal, hex and decimal
    long long toLongLong(uint16_t num, long long defaultValue = 0) const;

    template<class T>
    T toIntMinMax(uint16_t num, T min, T max, T defaultValue = 0) {
        if (!exists(num)) {
            return min;
        }
        return std::max(min, std::min(max, (T)toLongLong(num, (long long)defaultValue)));
    }

    double toDouble(uint16_t num, double defaultValue = 0) const;
    inline float toFloat(uint16_t num, float defaultValue = 0) const {
        return toDouble(num, defaultValue);
    }

    template<class T>
    T toFloatMinMax(uint16_t num, T min, T max, T defaultValue = 0) {
        if (!exists(num)) {
            return min;
        }
        return std::max(min, std::min(max, (T)toDouble(num, defaultValue)));
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
    uint32_t toMillis(uint16_t num, uint32_t minTime = 0, uint32_t maxTime = ~0, uint32_t defaultValue = 0, const String &defaultSuffix = String()) const;

    int toChar(uint16_t num, int defaultValue = -1) const;

    int toLowerChar(uint16_t num, int defaultValue = -1) const {
        return tolower(toChar(num, defaultValue));
    }

    inline String toString(uint16_t num) const {
        auto str = get(num);
        if (!str) {
            return String();
        }
        return str;
    }

    bool equalsIgnoreCase(uint16_t num, const __FlashStringHelper *str) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return strcasecmp_P(arg, RFPSTR(str)) == 0;
    }

    bool equalsIgnoreCase(uint16_t num, const String &str) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return strcasecmp(arg, str.c_str()) == 0;
    }

    bool equals(uint16_t num, const __FlashStringHelper *str) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return strcmp_P(arg, RFPSTR(str)) == 0;
    }

    bool equals(uint16_t num, const String &str) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return strcmp(arg, str.c_str()) == 0;
    }

    bool equals(uint16_t num, char ch) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return arg[0] == ch && arg[1] == 0;
    }

    bool isAnyMatchIgnoreCase(uint16_t num, const __FlashStringHelper *strings, char sep = STRINGLIST_SEPARATOR) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        return _isAnyMatchIgnoreCase(arg, strings, sep);
    }

    // return true for "", "*", "any", "all"
    bool isAny(uint16_t num) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        if (_isAnyMatchIgnoreCase(arg, F("" STRLS "*" STRLS "*.*" STRLS "any" STRLS "all"))) {
            return true;
        }
        return false;

    }

    // return true for "yes", "Y", "true", "on", "enable" and any integer != 0
    // arguments out of range return false
    bool isTrue(uint16_t num) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num)) || *arg == 0) {
            return false;
        }
        int result = 0;
        if ((_isValidInt(arg, result) && result != 0) || (_isAnyMatchIgnoreCase(arg, F("yes" STRLS "y" STRLS "true" STRLS "on" STRLS "enable")))) {
            return true;
        }
        return false;
    }

    // return true for "", "no", "N", "false", "off", "disable" and any integer == 0
    // arguments out of range return false
    bool isFalse(uint16_t num) const {
        ArgumentPtr arg;
        if (nullptr == (arg = get(num))) {
            return false;
        }
        int result;
        if (*arg == 0 || (_isValidInt(arg, result) && result == 0) || (_isAnyMatchIgnoreCase(arg, F("no" STRLS "n" STRLS "false" STRLS "off" STRLS "disable")))) {
            return true;
        }
        return false;
    }

public:
    bool requireArgs(uint16_t min, uint16_t max = ~0) {
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

public:
    void setCommand(const char *command) {
        _command = command;
        _command.toUpperCase();
    }
    String &getCommand() {
        return _command;
    }

    bool isCommand(const ATModeCommandHelp_t *help) const {
        auto ptr = _command.c_str();
        if (help->commandPrefix) {
            const auto len = strlen_P(help->commandPrefix);
            if (strncasecmp_P(ptr, help->commandPrefix, len)) {
                return false;
            }
            ptr += len;

        }
        return !strcasecmp_P(ptr, help->command);
    }

    bool isCommand(const __FlashStringHelper *command) const {
        return !strcasecmp_P(_command.c_str(), RFPSTR(command));
    }

    void printf_P(PGM_P format, ...);
    void print(const char *str);
    void print(const __FlashStringHelper *str);
    void print();
    void ok();

private:
    template <class T>
    bool _isValidInt(const char *str, T &result) const {
        char *endPtr = nullptr;
        auto value = (T)strtoll(str, &endPtr, 10);
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
            result = (T)value;
            return true;
        }
        // some more text or numbers
        return false;
    }

    bool _isAnyMatchIgnoreCase(String str, const __FlashStringHelper *strings, char sep = STRINGLIST_SEPARATOR) const {
        str.trim();
        str.toLowerCase();
        return (stringlist_find_P_P(RFPSTR(strings), str.c_str(), sep) != -1);
    }

private:
    Stream &_output;
    String _command;
    ArgumentVector _args;
    bool _queryMode;
};

#endif
