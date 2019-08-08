/**
 * Author: sascha_lammers@gmx.de
 */

#if AT_MODE_SUPPORTED

#pragma once

#ifndef DEBUG_AT_MODE
#define DEBUG_AT_MODE 0
#endif

#ifndef AT_MODE_ALLOW_PLUS_WITHOUT_AT
#define AT_MODE_ALLOW_PLUS_WITHOUT_AT 1     // treat + and AT+ the same
#endif

typedef struct {
    PGM_P command;
    PGM_P arguments;
    PGM_P help;
    PGM_P helpQueryMode;
} ATModeCommandHelp_t;

#define PROGMEM_AT_MODE_HELP_COMMAND(name)          _at_mode_progmem_command_help_command_##name
#define PROGMEM_AT_MODE_HELP_COMMAND_T(name)        &_at_mode_progmem_command_help_t_##name

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF(name, str1, str2, str3, str4) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { str1 }; \
    static const char _at_mode_progmem_command_help_arguments_##name[] PROGMEM = { str2 }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { str3 }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { str4 }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        _at_mode_progmem_command_help_arguments_##name, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(name, str3, str4) \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { str3 }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { str4 }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        nullptr, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(name, str1, str2, str3) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { str1 }; \
    static const char _at_mode_progmem_command_help_arguments_##name[] PROGMEM = { str2 }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { str3 }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        _at_mode_progmem_command_help_arguments_##name, \
        _at_mode_progmem_command_help_help_##name, \
        nullptr \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(name, str1, str3) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { str1 }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { str3 }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        nullptr \
    };

#define PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPP(name, str1, str3, str4) \
    static const char _at_mode_progmem_command_help_command_##name[] PROGMEM = { str1 }; \
    static const char _at_mode_progmem_command_help_help_##name[] PROGMEM = { str3 }; \
    static const char _at_mode_progmem_command_help_help_query_mode_##name[] PROGMEM = { str4 }; \
    static const ATModeCommandHelp_t _at_mode_progmem_command_help_t_##name PROGMEM = { \
        _at_mode_progmem_command_help_command_##name, \
        nullptr, \
        _at_mode_progmem_command_help_help_##name, \
        _at_mode_progmem_command_help_help_query_mode_##name \
    };

void at_mode_setup();
void at_mode_add_help(const ATModeCommandHelp_t *help);
void serial_handle_event(String command);
String at_mode_print_command_string(Stream &output, char separator, bool trailingSeparator = true);
void at_mode_serial_input_handler(uint8_t type, const uint8_t *buffer, size_t len);
void at_mode_print_invalid_arguments(Stream &output);
void at_mode_print_ok(Stream &output);
void enable_at_mode(Stream &output);
void disable_at_mode(Stream &output);

#endif
