 /**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <../include/Syslog.h>
#include <ReadADC.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <StreamString.h>
#include <BitsToStr.h>
#include <Cat.h>
#include <vector>
#include <queue>
#include <JsonTools.h>
#include <ListDir.h>
#include <DumpBinary.h>
#include "at_mode.h"
#include "kfc_fw_config.h"
#include "fs_mapping.h"
#include "logger.h"
#include "misc.h"
#include "deep_sleep.h"
#include "save_crash.h"
#include "web_server.h"
#include "web_socket.h"
#include "async_web_response.h"
#include "serial_handler.h"
#include "blink_led_timer.h"
#include "plugins.h"
#include "PinMonitor.h"
#include "../src/plugins/plugins.h"
#include <stl_ext/memory.h>

#if __LED_BUILTIN_WS2812_NUM_LEDS
#    include <NeoPixelEx.h>
#endif

#if ESP8266
#    include <umm_malloc/umm_malloc.h>
     extern "C" {
#    include <umm_malloc/umm_local.h>
#    if ARDUINO_ESP8266_MAJOR == 3
#        include <umm_malloc/umm_heap_select.h>
#    endif
     }
#    include <core_esp8266_waveform.h>
#    include <core_version.h>
#    if ARDUINO_ESP8266_MAJOR == 0
#        error Invalid coe comnfig
#   endif
#endif

#if ESP32
#    include "esp32_perfmon.hpp"
#endif

#if DEBUG_AT_MODE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;
using KFCConfigurationClasses::MainConfig;

#include <AsyncWebSocket.h>

static constexpr bool kCommandParserModeAllowShortPrefix = true;
static constexpr bool kCommandParserModeAllowNoPrefix = true;
static constexpr uint8_t kCommandParserMaxArgs = AT_MODE_MAX_ARGUMENTS;

extern void __kfcfw_queue_monitor(AsyncWebSocketMessage *dataMessage, AsyncClient *_client, AsyncWebSocket *_server);

void __kfcfw_queue_monitor(AsyncWebSocketMessage *dataMessage, AsyncClient *_client, AsyncWebSocket *_server)
{
    #if 0
        Serial.printf_P(PSTR("+WSQ: count=%u size=%u [%u:%u]"), _server->_getQueuedMessageCount(), _server->_getQueuedMessageSize(), WS_MAX_QUEUED_MESSAGES, WS_MAX_QUEUED_MESSAGES_SIZE);
    #if WS_MAX_QUEUED_MESSAGES_MIN_HEAP
        Serial.printf_P(PSTR(" heap %u/%u [%u:%u]"), ESP.getFreeHeap(), WS_MAX_QUEUED_MESSAGES_MIN_HEAP, WS_MIN_QUEUED_MESSAGES, WS_MIN_QUEUED_MESSAGES_SIZE);
    #endif
        Serial.println();
    #endif
}

#if HAVE_I2CSCANNER
#   include "i2c_scanner.h"
#endif

#if AT_MODE_HELP_SUPPORTED

typedef std::vector<ATModeCommandHelp> ATModeHelpVector;

static ATModeHelpVector *atModeCommandHelp = nullptr;

ATModeCommandHelp::ATModeCommandHelp(const ATModeCommandHelp_t *data, PGM_P pluginName) : _data(data), _name(pluginName)
{
}

PGM_P ATModeCommandHelp::command() const
{
    return _data->command;
}

PGM_P ATModeCommandHelp::commandPrefix() const
{
    return _data->commandPrefix;
}

PGM_P ATModeCommandHelp::arguments() const
{
    return _data->arguments;
}

PGM_P ATModeCommandHelp::help() const
{
    return _data->help;
}

PGM_P ATModeCommandHelp::helpQueryMode() const
{
    return _data->helpQueryMode;
}

PGM_P ATModeCommandHelp::pluginName() const
{
    return _name;
}

void ATModeCommandHelp::setPluginName(const __FlashStringHelper *name)
{
    _name = RFPSTR(name);
}

void ATModeCommandHelp::setPluginName(PGM_P name)
{
    _name = name;
}

void at_mode_add_help(const ATModeCommandHelp_t *help, PGM_P pluginName)
{
    atModeCommandHelp->emplace_back(help, pluginName);
}

void at_mode_display_help_indent(Stream &output, PGM_P text)
{
    PGM_P indent = PSTR("    ");
    uint8_t ch;
    ch = pgm_read_byte(text++);
    if (ch) {
        output.print(FPSTR(indent));
        do {
            output.print((char)ch);
            if (ch == '\n') {
                output.print(FPSTR(indent));
            }
        } while((ch = pgm_read_byte(text++)));
    }
    output.println();
}

// append PROGMEM strings to output and replace any whitespace with a single space
static void _appendHelpString(String &output, PGM_P str)
{
    if (str && pgm_read_byte(str)) {
        char lastChar = 0;
        if (output.length()) {
            lastChar = output.charAt(output.length() - 1);
        }

        char ch;
        while(0 != (ch = pgm_read_byte(str++))) {
            if (isspace(ch)) {
                if (!isspace(lastChar)) {
                    lastChar = ch;
                    output += ' ';
                }
            }
            else {
                lastChar = tolower(ch);
                output += lastChar;
            }
        }
        if (!isspace(lastChar)) {
            output += ' ';
        }
    }
}

void at_mode_display_help(Stream &output, StringVector *findText = nullptr)
{
    #if DEBUG_AT_MODE
        __LDBG_printf("size=%d, find=%s", atModeCommandHelp->size(), findText ? (findText->empty() ? PSTR("count=0") : implode(',', *findText).c_str()) : SPGM(null));
    #endif
    if (findText && findText->empty()) {
        findText = nullptr;
    }
    for(const auto &commandHelp: *atModeCommandHelp) {

        if (findText) {
            bool result = false;
            String tmp; // create single line text blob

            if (commandHelp.pluginName()) {
                tmp += F("plugin "); // allows to search for "plugin sensor"
                _appendHelpString(tmp, commandHelp.pluginName());
            }
            if (commandHelp.commandPrefix() && commandHelp.command()) {
                String str(commandHelp.getFPCommandPrefix());
                str.toLowerCase();
                tmp += str;
            }
            _appendHelpString(tmp, commandHelp.command());
            _appendHelpString(tmp, commandHelp.arguments());
            _appendHelpString(tmp, commandHelp.help());
            _appendHelpString(tmp, commandHelp.helpQueryMode());

            __LDBG_printf("find in %u: '%s'", tmp.length(), tmp.c_str());
            for(auto str: *findText) {
                if (tmp.indexOf(str) != -1) {
                    result = true;
                    break;
                }
            }
            if (!result) {
                continue;
            }
        }

        if (commandHelp.helpQueryMode()) {
            output.print(F(" AT"));
            if (commandHelp.command()) {
                output.print('+');
                if (commandHelp.commandPrefix()) {
                    output.print(commandHelp.getFPCommandPrefix());
                }
                output.print(commandHelp.getFPCommand());
            }
            output.println('?');
            at_mode_display_help_indent(output, commandHelp.helpQueryMode());
        }

        output.print(F(" AT"));
        if (commandHelp.command()) {
            output.print('+');
            if (commandHelp.commandPrefix()) {
                output.print(commandHelp.getFPCommandPrefix());
            }
            output.print(commandHelp.getFPCommand());
        }
        if (commandHelp.arguments()) {
            PGM_P arguments = commandHelp.arguments();
            auto ch = pgm_read_byte(arguments);
            if (ch == '[') {
                output.print('[');
                arguments++;
            }
            if (ch != '=') {
                output.print('=');
            }
            output.print(FPSTR(arguments));
        }
        output.println();

        at_mode_display_help_indent(output, commandHelp.help());
    }
}

#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(AT, "Print OK", "Show help");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HELP, "HELP", "[single][,word][,or entire phrase]", "Search help");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DSLP, "DSLP", "[<milliseconds>[,<mode>]]", "Enter deep sleep");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RST, "RST", "[<s>]", "Soft reset. 's' enables safe mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CMDS, "CMDS", "Send a list of available AT commands");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(LOAD, "LOAD", "Discard changes and load settings from EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(IMPORT, "IMPORT", "<filename|set_dirty>[,<handle>[,<handle>,...]]", "Import settings from JSON file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(STORE, "STORE", "Store current settings in EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FACTORY, "FACTORY", "Restore factory settings (but do not store in EEPROM)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FSR, "FSR", "FACTORY, STORE, RST in sequence");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ATMODE, "ATMODE", "<1|0>", "Enable/disable AT Mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DLY, "DLY", "<milliseconds>", "Call delay(milliseconds)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CAT, "CAT", "<filename>", "Display text file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RM, "RM", "<filename>", "Delete file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RN, "RN", "<filename>,<new filename>", "Rename file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LS, "LS", "[<directory>[,<hidden=true|false>,<subdirs=true|false>]]", "List files and directories");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LSR, "LSR", "[<directory>]", "List files and directories using FS.openDir()");

// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "[<reset|on|off|ap_on|ap_off|ap_standby|wimo|list|cfg>][,<wimo-mode 0=off|1=STA|2=AP|3=STA+AP>|clear[_flash]|diag[nostics]]", "Modify WiFi settings, wimo sets mode and reboots");

enum class WiFiCommandsType : uint8_t {
    RESET = 0,
    ST_ON,
    ST_OFF,
    ST_LIST,
    ST_CFG,
    AP_ON,
    AP_OFF,
    AP_STBY,
    DIAG,
    AVAIL_ST_LIST,
    NEXT,
};

#define WIFI_COMMANDS "reset|on|off|list|cfg|ap_on|ap_off|ap_standby|diag|stl|next"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "<" WIFI_COMMANDS ">", "Manage WiFi\n"
    "    reset                                       Reset WiFi connection\n"
    "    on                                          Enable WiFi station mode\n"
    "    off                                         Disable WiFi station mode\n"
    "    list[,<show passwords>]                     List WiFi networks. \n"
    "    cfg,[<...>]                                 Configure WiFi network\n"
    "    ap_on                                       Enable WiFi AP mode\n"
    "    ap_off                                      Disable WiFi AP mode\n"
    "    ap_standby                                  Set AP to stand-by mode (turns AP mode on if station mode cannot connect)\n"
    "    diag                                        Print diagnostic information\n"
    "    stl                                         List available WiFi stations\n"
    "    next                                        Switch to next WiFi station\n"
);

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(REM, "REM", "Ignore comment");
#if __LED_BUILTIN_WS2812_NUM_LEDS
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(NEOPX, "NEOPX", "<pin>,<num>,<r>,<g>,<b>", "Set NeoPixel color for given pin");
#endif
#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LED, "LED", "<slow,fast,flicker,off,solid,sos,pattern>,[,color=0xff0000|pattern=10110...][,pin]", "Set LED mode");
#endif

#if RTC_SUPPORT
PROGMEM_AT_MODE_HELP_COMMAND_DEF(RTC, "RTC", "[<set>]", "Set RTC time", "Display RTC time");
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PLG, "PLG", "<list|start|stop|add-blacklist|add|remove>[,<name>]", "Plugin management");

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DSH, "DSH", "Display serial handler");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FSM, "FSM", "Display FS mapping");
#if PIN_MONITOR
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PINM, "PINM", "[<1=start|0=stop>,<interval in ms>]", "List or monitor PINs");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RSSI, "RSSI", "[interval in seconds|0=disable]", "Display WiFi RSSI");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(GPIO, "GPIO", "[interval in seconds|0=disable]", "Display GPIO states");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PWM, "PWM", "<pin>,<input|input_pullup|waveform|level=0-" __STRINGIFY(PWMRANGE) ">[,<frequency=100-40000Hz>[,<duration/ms>]]", "PWM output on PIN, min./max. level set it to LOW/HIGH");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ADC, "ADC", "<off|display interval=1s>[,<period=1s>,<multiplier=1.0>,<unit=mV>,<read delay=5000us>]", "Read the ADC and display values");
#if ESP32
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CPU, "CPU", "Toggle displaying CPU usage");
#endif
#if defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CPU, "CPU", "<80|160>", "Set CPU speed", "Display CPU speed");
#endif


#if DEBUG

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PSTORE, "PSTORE", "[<clear|remove|add>[,<key>[,<value>]]]", "Display/modify persistent storage");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(METRICS, "METRICS", "Display system metrics");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMP, "DUMP", "[<dirty|config.name>]", "Display settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPT, "DUMPT", "Dump timers");
#if DEBUG_CONFIGURATION_GETHANDLE
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPH, "DUMPH", "[<log|panic|clear>]", "Dump configuration handles");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPIO, "DUMPIO", "<address=0x60000000>[,<end address|length=4>]", "Dump IO memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPM, "DUMPM", "<address>[,<length=32>][,<insecure=false>,<use ESP.flashRead()=true>]", "Dump memory (32bit aligned)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPF, "DUMPF", "<start=0x40200000>[,<end address|length=32>]", "Dump flash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(FLASH, "FLASH", "<e[rase]>,<address>|<r[ead]>,<address>[,<offset=0>,<length=4096>]|w[rite],<address>,<byte1>[,<byte2>[,...]]]", "Erase, read or write flash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPFS, "DUMPFS", "Display file system information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RTCM, "RTCM", "<list|dump|clear|set|get|quickconnect>[,<id>[,<data>]", "RTC memory access");
#if LOGGER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOGDBG, "LOGDBG", "<1|0>", "Enable/disable writing debug output to log://debug");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PANIC, "PANIC", "[<address|wdt|hwdt|alloc>]", "Cause an exception by calling panic(), writing zeros to memory <address> or triggering the (hardware)WDT");

#endif

#if HAVE_I2CSCANNER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CSCAN, "I2CSCAN", "[<start-address=1>][,<end-address=127>][,<sda=4|any|no-init>,<scl=5>]", "Scan I2C bus. If ANY is passed as third argument, all available PINs are probed for I2C devices");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CS, "I2CS", "<pin-sda>,<pin-scl>[,<speed=100000>,<clock-stretch=45000>,<start|stop>]", "Configure I2C");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CTM, "I2CTM", "<address>,<data,...>", "Transmsit data to slave");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CRQ, "I2CRQ", "<address>,<length>", "Request data from slave");
#if ENABLE_ARDUINO_OTA
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(AOTA, "AOTA", "<start|stop>", "Start/stop Arduino OTA");
#endif

#if AT_MODE_HELP_SUPPORTED

void at_mode_help_commands()
{
    auto name = PSTR("at_mode");
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(AT), name);
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HELP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DSLP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RST), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(CMDS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LOAD), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(IMPORT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(STORE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(FACTORY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(FSR), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(ATMODE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DLY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(CAT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RN), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LSR), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(WIFI), name);
#if __LED_BUILTIN_WS2812_NUM_LEDS
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(NEOPX), name);
#endif
#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LED), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(REM), name);
#if RTC_SUPPORT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RTC), name);
#endif

    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DSH), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(FSM), name);
#if PIN_MONITOR
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PINM), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PLG), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HEAP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RSSI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(GPIO), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PWM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(ADC), name);
#if ESP32
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(CPU), name);
#endif
#if defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(CPU), name);
#endif
#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PSTORE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(METRICS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMP), name);
#if DEBUG && ESP8266
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPT), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPIO), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPFS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(FLASH), name);
#if DEBUG_CONFIGURATION_GETHANDLE
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPH), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RTCM), name);
#if LOGGER
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LOGDBG), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PANIC), name);
#endif
#if HAVE_I2CSCANNER
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(I2CSCAN), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(I2CS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(I2CTM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(I2CRQ), name);
#if ENABLE_ARDUINO_OTA
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(AOTA), name);
#endif
}

static void new_ATModeHelpVector_atModeCommandHelp()
{
    #if DEBUG
        if (atModeCommandHelp) {
            __DBG_panic("atModeCommandHelp=%p", atModeCommandHelp);
        }
    #endif
    atModeCommandHelp = new ATModeHelpVector();
    if (!atModeCommandHelp) {
        __DBG_printf_E("memory allocation failed");
    }
}

void at_mode_generate_help(Stream &output, StringVector *findText = nullptr)
{
    __LDBG_printf("find=%s", findText ? implode(',', *findText).c_str() : PSTR("nullptr"));

    new_ATModeHelpVector_atModeCommandHelp();
    if (!atModeCommandHelp) {
        return;
    }

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin: PluginComponents::Register::getPlugins()) {
        plugin->atModeHelpGenerator();
    }
    at_mode_display_help(output, findText);

    delete atModeCommandHelp;
    atModeCommandHelp = nullptr;

    if (config.isSafeMode()) {
        output.printf_P(PSTR("\n%s\n\n"), PSTR("SAFE MODE ENABLED"));
    }
}

void at_mode_print_command_string(Stream &output, char separator)
{
    new_ATModeHelpVector_atModeCommandHelp();
    if (!atModeCommandHelp) {
        return;
    }

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin: PluginComponents::Register::getPlugins()) {
        plugin->atModeHelpGenerator();
    }

    uint16_t count = 0;
    for(const auto commandHelp: *atModeCommandHelp) {
        if (commandHelp.command()) {
            if (count++ != 0) {
                output.print(separator);
            }
            if (commandHelp.commandPrefix()) {
                output.print(commandHelp.getFPCommandPrefix());
            }
            output.print(commandHelp.getFPCommand());
        }
    }

    delete atModeCommandHelp;
    atModeCommandHelp = nullptr;
}

#endif

class DisplayTimer;

extern DisplayTimer *displayTimer;

#if 1

class DisplayTimer {
public:
    enum class DisplayType {
        HEAP = 1,
        RSSI = 2,
        GPIO = 3,
    };

    DisplayTimer() :
        _type(DisplayType::HEAP)
    {
        __DBG_assert_printf(displayTimer == nullptr, "displayTimer not null");
        stdex::reset(displayTimer, this);
    }
    ~DisplayTimer() {
        if (this == displayTimer) {
            displayTimer = nullptr;
        }
    }

    void setType(DisplayType type, Event::milliseconds interval) {
        _type = type;
        if (_type == DisplayType::HEAP) {
            LOOP_FUNCTION_ADD(loop);
        }
        else {
            LoopFunctions::remove(loop);
        }
        _Timer(_timer).add(interval, true, DisplayTimer::printTimerCallback);
        _maxHeap = 0;
        _minHeap = ESP.getFreeHeap();
        _rssiMin = std::numeric_limits<decltype(_rssiMin)>::min();
        _rssiMax = 0;
    }

    DisplayType getType() const {
        return _type;
    }

    void printHeap() {
        #if ESP32
            Serial.printf_P(PSTR("+HEAP: heap=%u(min=%u/max=%u) psram=%u(min=%u) cpu=%dMHz uptime=%us\n"),
                ESP.getFreeHeap(),
                _minHeap,
                _maxHeap,
                ESP.getFreePsram(),
                ESP.getMinFreePsram(),
                ESP.getCpuFreqMHz(),
                getSystemUptime()
            );
        #else
            Serial.printf_P(PSTR("+HEAP: free=%u(min=%u/max=%u) cpu=%dMHz frag=%u uptime=%us\n"),
                ESP.getFreeHeap(),
                _minHeap,
                _maxHeap,
                ESP.getCpuFreqMHz(),
                ESP.getHeapFragmentation(),
                getSystemUptime()
            );
            #if defined(UMM_STATS) || defined(UMM_STATS_FULL)
                umm_print_stats(2);
            #endif
        #endif
    }

    void printGPIO() {
        Serial.printf_P(PSTR("+GPIO: "));
        #if defined(ESP8266)
            for(uint8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
                if (i == 10 || (i != 1 && !isFlashInterfacePin(i))) { // do not display TX and flash SPI
                    // pinMode(i, INPUT);
                    Serial.printf_P(PSTR("%u=%u "), i, digitalRead(i));
                    #if ESP8266
                        if (i == 16) {
                            Serial.print((GP16E & 1) ? F("IN ") : F("OUT "));
                        }
                        else {
                            String tmp;
                            tmp = GPO & (1 << i) ? F("OUT") : F("IN");
                            tmp += GPF(i) & (1 << GPFPU) ? F("_PULLUP ") : F(" ");
                            Serial.print(tmp);
                        }
                    #endif
                }
            }
            Serial.printf_P(PSTR("A0=%u\n"), analogRead(A0));
        #elif defined(ESP32)
            for(uint8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
                Serial.printf_P(PSTR("%u=%u%c"), i, digitalRead(i), (i == NUM_DIGITAL_PINS - 1) ? '\n' : ' ');
            }
            // Serial.println(PinMonitor::GPIO::read(), 2);
            // static const uint8_t pins[] PROGMEM = {36, 39};
            // auto ptr = pins;
            // for(uint8_t i = 0; i < sizeof(pins); i++) {
            //     auto pin = pgm_read_byte(ptr++);
            //     Serial.printf_P(PSTR(" A%u=%u"), pin, analogRead(pin));

            // }
            // Serial.println();

        #endif
        #if defined(HAVE_IOEXPANDER)
            IOExpander::config.dumpPins(Serial);
        #endif
    }

    void printRSSI() {
        int16_t rssi = WiFi.RSSI();
        _rssiMin = std::max(_rssiMin, rssi);
        _rssiMax = std::min(_rssiMax, rssi);
        Serial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, _rssiMin, _rssiMax);
    }

    void print() {
        switch(_type) {
            case DisplayType::HEAP:
                printHeap();
                break;
            case DisplayType::GPIO:
                printGPIO();
                break;
            case DisplayType::RSSI:
                printRSSI();
            default:
                break;
        }
    }

    static void printTimerCallback(Event::CallbackTimerPtr timer) {
        if (displayTimer == nullptr) {
            timer->disarm();
            return;
        }
        displayTimer->print();
    }

    bool removeTimer() {
        return _Timer(_timer).remove();
    }

    void remove() {
        _Timer(_timer).remove();
        LoopFunctions::remove(loop);
        delete this;
    }

    void _loop() {
        _minHeap = std::min<uint32_t>(ESP.getFreeHeap(), _minHeap);
        _maxHeap = std::max<uint32_t>(ESP.getFreeHeap(), _maxHeap);
    }

    static void loop() {
        if (displayTimer) {
            displayTimer->_loop();
        }
    }

private:
    Event::Timer _timer;
    DisplayType _type;
    int16_t _rssiMin;
    int16_t _rssiMax;
    uint32_t _maxHeap;
    uint32_t _minHeap;
};

DisplayTimer *displayTimer;

static void print_heap()
{
    if (displayTimer) {
        displayTimer->printHeap();
    }
    else {
        Serial.printf_P(PSTR("+HEAP: free=%u cpu=%dMHz frag=%u"), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), ESP.getHeapFragmentation());
    }
}

#endif

void at_mode_wifi_callback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        Serial.printf_P(PSTR("WiFi connected to %s - IP %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        Serial.println(F("WiFi connection lost"));
    }
}

SerialHandler::Client *_client;
bool is_at_mode_enabled;

bool at_mode_enabled()
{
    return is_at_mode_enabled;
}

void at_mode_setup()
{
    is_at_mode_enabled = System::Flags::getConfig().is_at_mode_enabled;
    __LDBG_printf("AT_MODE_ENABLED=%u", is_at_mode_enabled);

    if (_client) {
        serialHandler.removeClient(*_client);
    }
    __LDBG_printf("installing serial handler");
    _client = &serialHandler.addClient(at_mode_serial_input_handler, SerialHandler::EventType::READ);

    __LDBG_printf("adding wificallback");
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, at_mode_wifi_callback);
    __LDBG_printf("setup done");
}

void enable_at_mode(Stream *output)
{
    if (!is_at_mode_enabled) {
        if (output) {
            output->println(F("Enabling AT MODE."));
        }
        is_at_mode_enabled = true;
        // if (!_client) {
        //     _client = &serialHandler.addClient(at_mode_serial_input_handler, SerialHandler::EventType::READ);
        // }
        // _client->start(SerialHandler::EventType::READ);
    }
}

void disable_at_mode(Stream *output)
{
    if (is_at_mode_enabled) {
        // if (_client) {
        //     serialHandler.removeClient(*_client);
        //     _client = nullptr;
        // }
        if (output) {
            output->println(F("Disabling AT MODE."));
        }
        #if DEBUG
            if (displayTimer) {
                displayTimer->remove();
            }
        #endif
        is_at_mode_enabled = false;
    }
}

void at_mode_dump_fs_info(Stream &output)
{
    FSInfo info;
    KFCFS.info(info);
    output.printf_P(PSTR(
        "+FS: Block size           %d\n"
        "+FS: Max. open files      %d\n"
        "+FS: Max. path length     %d\n"
        "+FS: Page size            %d\n"
        "+FS: Total bytes          %d\n"
        "+FS: Used bytes           %d (%.2f%%)\n"),
        info.blockSize, info.maxOpenFiles, info.maxPathLength, info.pageSize, info.totalBytes, info.usedBytes, info.usedBytes * 100.0 / info.totalBytes
    );
}

void at_mode_print_help(Stream &output)
{
    #if AT_MODE_HELP_SUPPORTED
        output.println(F("AT? or AT+HELP=<command|text to find> for help"));
    #else
        output.println(F("try https://github.com/sascha432/esp8266-kfc-fw/blob/master/docs/AtModeHelp.md\n"));
    #endif
    if (config.isSafeMode()) {
        output.println(F("SAFE MODE ENABLED"));
    }
}

void at_mode_print_invalid_command(Stream &output)
{
    output.print(F("ERROR - Invalid command. "));
    at_mode_print_help(output);
}

void at_mode_print_invalid_arguments(Stream &output, uint16_t num, uint16_t min, uint16_t max)
{
    static constexpr uint16_t UNSET = ~0;
    output.print(F("ERROR - "));
    if (min != UNSET) {
        if (min == max || max == UNSET) {
            output.printf_P(PSTR("Expected %u argument(s), got %u\n"), min, num);
        }
        else {
            output.printf_P(PSTR("Expected %u to %u argument(s), got %u\n"), min, max, num);
        }
    }
    else {
        output.println(F("Invalid arguments"));
    }
    at_mode_print_help(output);
}

void at_mode_print_prefix(Stream &output, const __FlashStringHelper *command)
{
    output.print('+');
    output.print(command);
    output.print(F(": "));
}

void at_mode_print_prefix(Stream &output, const char *command)
{
    output.printf_P(PSTR("+%s: "), command);
}

static void at_mode_adc_loop();
static void at_mode_adc_delete_object();

class AtModeADC {
public:
    AtModeADC() : _adcIntegralMultiplier(0), _adc(ADCManager::getInstance()) {
    }
    virtual ~AtModeADC() {
        _Timer(_displayTimer).remove();
        if (_adcIntegralMultiplier) {
            LoopFunctions::remove(at_mode_adc_loop);
        }
    }

    // useable values for integrating the average value is 1-100 (averagePeriodMillis)
    bool init(uint16_t averagePeriodMillis, float convertMultiplier, const String &unit, uint32_t readDelayMicros = 5000) {
        _Timer(_displayTimer).remove();
        if (!averagePeriodMillis) {
            return false;
        }
        _unit = unit;
        _convMultiplier = convertMultiplier;
        _adcIntegralMultiplier = 1.0 / (1000.0 / averagePeriodMillis);
        _adcIntegral = 0;
        _adcIntegral2 = 0;
        _timerSum = 0;
        _timerCount = 0;
        _lastUpdate = 0;
        _readDelay = readDelayMicros;
        LOOP_FUNCTION_ADD(at_mode_adc_loop);
        _timer.start();
        return true;
    }

    String getConvertedString() const {
        int prec = 0;
        prec = (_convMultiplier >= 10) ? 0 : ((_convMultiplier < 0.1) ? 6 : (_convMultiplier < 1) ? 3 : 1);
        return PrintString(F("%.*f%s"), prec, _adcIntegral * _convMultiplier, _unit.c_str());
    }

    uint16_t getValue() const {
        return round(_adcIntegral);
    }

    void printInfo(Print &output) {
        output.printf_P("value=%.1f value2=%.1f time=%uus (avg) samples=%u multiplier=%.6f\n", _adcIntegral, _adcIntegral2, _timerSum / _timerCount, _timerCount, _adcIntegralMultiplier);
        _adcIntegral2 = 0;
        _timerSum = 0;
        _timerCount = 0;
    }

private:
    virtual void processData(uint16_t reading, uint32_t diff, uint32_t micros) {
        float count = diff * _adcIntegralMultiplier;
        _adcIntegral = ((count * _adcIntegral) + (double)reading) / (count + 1.0);
        _adcIntegral2 = ((1.0 * _adcIntegral2) + (double)reading) / 2.0;
        _timerSum += diff;
        _timerCount++;
        if (_timerSum > 30 * 1000 * 1000) {
            _timerSum /= 2;
            _timerCount /= 2;
        }
    }

public:
    void loop() {
        auto time = micros();
        auto diff = _timer.getTime(time);
        if (diff < _readDelay) {
            return; // wait for the next loop
        }
        uint32_t tmp;
        auto reading = _adc.readValue(tmp);
        if (tmp == _lastUpdate) {
            return;
        }
        _lastUpdate = tmp;

        _timer.start();
        processData(reading, diff, time);
    }

    Event::Timer &getTimer() {
        return _displayTimer;
    }

protected:
    union {
        struct {
            float _convMultiplier;
            float _adcIntegral;
            float _adcIntegral2;
            uint32_t _timerSum;
            uint32_t _timerCount;
        };
        struct {
            uint32_t duration;
            uint32_t start;
            uint16_t packetSize;
            uint32_t sent;
            uint32_t dropped;
        } _webSocket;
    };
    uint32_t _readDelay;
    uint32_t _lastUpdate;
    float _adcIntegralMultiplier;
    String _unit;
    MicrosTimer _timer;
    Event::Timer _displayTimer;
    ADCManager &_adc;
};

class AtModeADCWebSocket : public AtModeADC
{
public:
    using AtModeADC::AtModeADC;

    bool init(uint32_t interval, uint32_t duration, uint16_t packetSize, AsyncWebSocketClient *client) {
        _Timer(_displayTimer).remove();
        if (!interval) {
            return false;
        }
        _adcIntegralMultiplier = 1;
        _webSocket.duration = duration;
        _webSocket.start = micros() / 1000U;
        _webSocket.packetSize = packetSize;
        _webSocket.dropped = 0;
        _webSocket.sent = 0;
        _client = client;

        resetBuffer();

        _lastUpdate = 0;
        _readDelay = interval;
        LOOP_FUNCTION_ADD(at_mode_adc_loop);
        _timer.start();
        return true;
    }

private:
    typedef struct __attribute__packed__ {
        uint32_t _time: 22;
        uint32_t _value: 10;
        uint16_t _value2;
    } Data_t;

    typedef struct __attribute__packed__ {
        WsClient::BinaryPacketType type;
        uint16_t flags;
        uint8_t packet_size;
    } Header_t;

    static constexpr uint16_t FLAGS_NONE = 0x0000;
    static constexpr uint16_t FLAGS_LAST_PACKET = 0x0001;
    static constexpr uint16_t FLAGS_DROPPED_PACKETS = 0x0002;

    void resetBuffer()
    {
        _buffer.setLength(0);
        _buffer.reserve(_webSocket.packetSize + 1);
        Header_t header = { WsClient::BinaryPacketType::ADC_READINGS, FLAGS_NONE, sizeof(Data_t) };
        _buffer.push_back(header);
    }

    virtual void processData(uint16_t reading, uint32_t diff, uint32_t micros) override
    {
        Data_t data;
        uint32_t ms = micros / 1000U;
        auto time = get_time_diff(_webSocket.start, ms);

        if (_buffer.length() + sizeof(data) >= _webSocket.packetSize) {

            bool endReading = (time > _webSocket.duration);

            if (Http2Serial::getClientById(_client)) {
                if (_client->canSend()) {
                    _buffer.write(0); // terminate with NUL byte
                    size_t len = _buffer.length() - 1;
                    auto ptr = _buffer.get();
                    _webSocket.sent += len;

                    if (len >= sizeof(Header_t)) {
                        auto flags = FLAGS_NONE;
                        if (endReading) {
                            flags |= FLAGS_LAST_PACKET;
                        }
                        if (_webSocket.dropped) {
                            flags |= FLAGS_DROPPED_PACKETS;
                        }
                        ((Header_t *)&ptr[0])->flags |= flags;
                    }

                    auto wsBuffer = _client->server()->makeBuffer(ptr, len);
                    if (wsBuffer) {
                        _client->binary(wsBuffer);
                    }
                }
                else {
                    _webSocket.dropped += _buffer.length();
                }
            }

            if (endReading) {
                _buffer.clear();
                Serial.printf_P(PSTR("+ADC: Finished client=%p %u bytes @ %.2fKB/s%s\n"),
                    _client,
                    _webSocket.sent,
                    _webSocket.sent * (1000.0 / 1024.0) / _webSocket.duration,
                    _webSocket.dropped ? PrintString(F(" %u bytes dropped"), _webSocket.dropped).c_str() : emptyString.c_str()
                );
                LoopFunctions::remove(at_mode_adc_loop);
                LoopFunctions::callOnce(at_mode_adc_delete_object);
                return;
            }

            resetBuffer();
        }

        data._time = time;
        data._value = std::min(reading, (uint16_t)1023);
        #if IOT_BLINDS_CTRL
            data._value2 = BlindsControlPlugin::getInstance().getCurrent();
        #else
            data._value2 = 0;
        #endif

        _buffer.push_back(data);
    }

private:
    Buffer _buffer;
    AsyncWebSocketClient *_client;
};

AtModeADC *atModeADC;

static void at_mode_adc_delete_object()
{
    stdex::reset(atModeADC);
}

static void at_mode_adc_loop()
{
    if (atModeADC) {
        atModeADC->loop();
    }
}

class StringSSOSize : public String
{
public:
    static size_t getSSOSize() {
        return String::SSOSIZE;
    }
};

class CommandQueue;

static CommandQueue *commandQueue = nullptr;
class CommandQueue {
public:
    CommandQueue(Stream &stream) : _output(stream), _queue(), _start(millis()), _commands(0) {
        _output.printf_P(PSTR("+QUEUE: started\n"));
        LOOP_FUNCTION_ADD_ARG([this]() { loop(); }, this);
    }

    virtual ~CommandQueue() {
        LoopFunctions::remove(this);
        _output.printf_P(PSTR("+QUEUE: finished, duration=%.2fs, commands=%u\n"), get_time_diff(_start, millis()) / 1000.0, _commands);
    }

    void loop() {
        if (!_timer) {
            if (_queue.empty()) {
                if (commandQueue == this) {
                    commandQueue = nullptr;
                    LoopFunctions::callOnce([this]() {
                        delete this;
                    });
                }
            }
            else {
                // auto args = std::move(_queue.front());
                // _queue.pop();
                // _commands++;
                // __DBG_printf("ARGS cmd=%s,argc=%d,args='%s'", args.getCommand().c_str(), args.size(), implode(F("','"), args.getArgs()).c_str());
                // at_mode_serial_handle_event(const_cast<String &>(emptyString), &args);
            }
        }
        else {
            __DBG_printf("delay timer active");
            delay(100);
        }
    }

    void setDelay(uint32_t delay) {
        __DBG_printf("delay %u", delay);
        _Timer(_timer).add(delay, false, [](Event::CallbackTimerPtr timer) {});
    }

    bool hasQueue() const {
        return (_timer == true || _queue.empty() == false);
    }

    void queueCommand(const AtModeArgs &args) {
        __DBG_printf("ADD queue command");
        __DBG_printf("ARGS cmd=%s,argc=%d,args='%s'", args.getCommand().c_str(), args.size(), implode(F("','"), args.getArgs()).c_str());
        _queue.push(args);
    }

private:
    using Queue = std::queue<AtModeArgs>;

    Stream &_output;
    Event::Timer _timer;
    Queue _queue;
    uint32_t _start;
    uint16_t _commands;
};

static bool tokenizerCmpFuncCmdLineMode(char ch, int type)
{
    if (type == 1) { // command separator
        if (ch == ' ') {
            return true;
        }
    }
    else if (type == 2 && ch == ';') { // new command / new line separator
        return true;
    }
    else if (type == 3 && ch == '"') { // quotes
        return true;
    }
    else if (type == 4 && ch == '\\') { // escape character
        return true;
    }
    else if (type == 5 && ch == ' ') { // token separator
        return true;
    }
    else if (type == 6 && isspace(ch)) { // leading whitespace outside quotes
        return true;
    }
    return false;
}

#if DEBUG

static bool _writeAndVerifyFlash(uint32_t address, uint8_t *data, size_t size, uint8_t *compare, const AtModeArgs &args)
{
    if (ESP.flashWrite(address, data, size) == false) {
        args.print(F("flash write error address=%08x length=%u"), address, size);
        return false;
    }
    if (ESP.flashRead(address, compare, size) == false) {
        args.print(F("flash read error address=%08x length=%u"), address, size);
        return false;
    }
    if (memcmp(data, compare, size) != 0) {
        args.print(F("flash verify error address=%08x length=%u"), address, size);
        return false;
    }
    return true;
}

static uintptr_t translateAddress(String str) {
    str.trim('_');
    #if !ESP32
        if (str.equalsIgnoreCase(F("text")) || str.equalsIgnoreCase(F("irom0_text_start"))) {
            return (uintptr_t)&_irom0_text_start;
        }
        else if (str.equalsIgnoreCase(F("irom0_text_end"))) {
            return (uintptr_t)&_irom0_text_start;
        }
        else if (str.startsWithIgnoreCase(F("heap"))) {
            return (uintptr_t)&_heap_start;
        }
        else if (str.startsWithIgnoreCase(F("fs_s")) || str.equalsIgnoreCase(F("fs"))) {
            return (uintptr_t)&_FS_start;
        }
        else if (str.startsWithIgnoreCase(F("fs_e"))) {
            return (uintptr_t)&_FS_end;
        }
        else if (str.startsWithIgnoreCase(F("nvs"))) {
            return (uintptr_t)&_NVS_start;
        }
        else if (str.startsWithIgnoreCase(F("nvs_e"))) {
            return (uintptr_t)&_NVS_end;
        }
        else if (str.startsWithIgnoreCase(F("nvs2"))) {
            return (uintptr_t)&_NVS2_start;
        }
        else if (str.startsWithIgnoreCase(F("nvs2_e"))) {
            return (uintptr_t)&_NVS2_end;
        }
        else if (str.startsWithIgnoreCase(F("savecrash"))) {
            return (uintptr_t)&_SAVECRASH_start;
        }
        else if (str.startsWithIgnoreCase(F("savecrash_e"))) {
            return (uintptr_t)&_SAVECRASH_end;
        }
        else if (str.startsWithIgnoreCase(F("ee"))) {
            return (uintptr_t)&_EEPROM_start;
        }
        else if (str.equalsIgnoreCase(F("gpi"))) {
            return (uintptr_t)&GPI;
        }
        else if (str.startsWithIgnoreCase(F("gpo"))) {
            return (uintptr_t)&GPO;
        }
    #endif
    return (uintptr_t)~0;
}

#endif

void at_mode_print_WiFi_info(AtModeArgs &args, uint8_t num, const Network::WiFi::StationModeSettings &cfg, bool showPassword = false)
{
    auto ssid = Network::WiFi::getSSID(num);
    bool isConfigured = (ssid && *ssid && cfg.isEnabled());
    bool isActive = (config.getWiFiConfigurationNum() == num);

    constexpr size_t kLineLength = 42;
    char line[kLineLength + 1];
    std::fill(std::begin(line), std::end(line), '-');
    line[kLineLength] = 0;
    args.print(F("%s"), line);

    args.print(F("Connection #%u%s"),
        num,
        isConfigured ? PrintString(F(" (Priority %s%s)"), cfg.getPriorityStr().c_str(), isActive ? (WiFi.isConnected() ? PSTR(", connected") : PSTR(", active")) : emptyString.c_str()).c_str() : PSTR(" (not configured)")
    );
    if (isConfigured) {
        if (ssid && *ssid) {
            args.print(    F("SSID      %s"), (ssid && *ssid) ? ssid : PSTR("<none>"));
            args.print(    F("Password  %s"), showPassword ? Network::WiFi::getPassword(num) : PSTR("*********"));
        }
        if (cfg.isDHCPEnabled()) {
            if (isActive && WiFi.isConnected()) {
                args.print(F("DHCP IP   %s"), WiFi.localIP().toString().c_str());
                args.print(F("Subnet    %s"), WiFi.subnetMask().toString().c_str());
                args.print(F("Gateway   %s"), WiFi.gatewayIP().toString().c_str());
                args.print(F("DNS       %s, %s"), WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str());
            }
            else {
                args.print(F("DHCP client enabled"));
            }
        }
        else {
            args.print(    F("Static IP %s"), cfg.getLocalIp().toString().c_str());
            args.print(    F("Subnet    %s"), cfg.getSubnet().toString().c_str());
            args.print(    F("Gateway   %s"), cfg.getGateway().toString().c_str());
            args.print(    F("DNS       %s, %s"), cfg.getDns1().toString().c_str(), cfg.getDns2().toString().c_str());
        }
    }
}

void at_mode_serial_handle_event(String &commandString)
{
    auto &output = Serial;
    char *nextCommand = nullptr;
    AtModeArgs args(output);
    bool atModeCommands = true;

    // determine if query mode by checking for a trailing '?'
    commandString.trim();
    bool isQueryMode = commandString.endsWith('?');

    // __dump_binary_to(output, commandString.c_str(), commandString.length(), 16, nullptr, 4);

    // check command prefix
    if (commandString.startsWithIgnoreCase(F("AT"))) {
        // remove AT from the command
        commandString.remove(0, 2);
    }
    else if (kCommandParserModeAllowShortPrefix && commandString.startsWith('+')) {
        // allow using AT+COMMAND[?|=<args,...>] and +COMMAND[?|=<args,...>]
    }
    else if (kCommandParserModeAllowNoPrefix) {
        // allow using COMMAND [<arg1>][ <args...>] (or /COMMAND, --COMMAND -COMMAND)
        atModeCommands = false;
    }
    else {
        // display invalid command if it does not start with the allowed prefix
        at_mode_print_invalid_command(output);
        return;
    }

    // check for empty commands
    if (commandString.length() == 0) { // AT
        if (atModeCommands) { // display OK for empty AT commands and ignore empty lines
            args.ok();
        }
        return;
    }

    #if AT_MODE_HELP_SUPPORTED

        // check if help is requested
        if (commandString == '?' || commandString == 'h' || commandString == F("/?") || commandString == F("/help")) {
            at_mode_generate_help(output);
            return;
        }

    #endif

    auto command = commandString.begin();
    // remove leading '+'
    if (*command == '+') {
        command++;
    }
    // removing leading '/', '-' and '--'
    else if (kCommandParserModeAllowNoPrefix && !atModeCommands) {
        if (*command == '/') {
            command++;
        }
        else if (*command == '-') {
            command++;
            if (*command == '-') {
                command++;
            }
        }
    }

    if (isQueryMode) {
        // remove trailing ?
        *strrchr(command,  '?') = 0;
        args.setQueryMode(true);
    }
    else if (atModeCommands) {
        // run tokenizer in AT command mode
        // command=arg1,arg2,arg3;command2=...
        __LDBG_printf("tokenizer('%s')", command);
        args.setQueryMode(false);
        tokenizer(command, args, true, &nextCommand);
        auto argsStr = implode(F("' '"), args.getArgs());
        __LDBG_printf("cmd=%s,argc=%d,args='%s',next_cmd='%s'", command, args.size(), argsStr.c_str(), __S(nextCommand));
    }
    else if __CONSTEXPR17 (kCommandParserModeAllowNoPrefix) {
        // run tokenizer in cli mode
        // command arg1 arg2 arg3 ; command2 ...

        args.setQueryMode(false);
        tokenizer(command, args, true, &nextCommand, tokenizerCmpFuncCmdLineMode);
        auto argsStr = implode(F("' '"), args.getArgs());
        __LDBG_printf("cmd=%s,argc=%u,args='%s',next_cmd='%s'", __S(command), args.size(), __S(implode(F("' '"), args.getArgs())), __S(nextCommand));
    }
    // store copy of command
    args.setCommand(command);

    #if DEBUG
        if (args.isCommand(F("TT"))) { // test tokenizer

            args.print(F("command='%s' args_num=%u query_mode=%u at_command_mode=%u"), args.getCommand().c_str(), args.size(), args.isQueryMode(), atModeCommands);
            for(uint8_t i = 0; i < args.size(); i++) {
                args.print(F("arg#=%u value='%s'"), i, args[i]);
            }

        }
        else
    #endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DSLP))) {
        KFCFWConfiguration::milliseconds time(args.toMillis(0));
        RFMode mode = (RFMode)args.toInt(1, RF_DEFAULT);

        #if ESP32

            args.print(F("Entering deep sleep... time=%ums"), time.count());
            ESP.deepSleep(time.count() * 1000ULL);

        #else
            //+dslp=15000,4

            args.print(F("Entering deep sleep... time=%ums deep_sleep_max=%.0fms mode=%u"), time.count(), (ESP.deepSleepMax() / 1000.0), mode);

            #if ENABLE_DEEP_SLEEP
                DeepSleep::deepSleepParams.enterDeepSleep(time, mode);
            #else
                WiFi.disconnect(true);
                ESP.deepSleep(time.count() * 1000ULL, mode);
                ESP.deepSleep(ESP.deepSleepMax() / 2, mode);
                ESP.deepSleep(0, mode);
            #endif
        #endif
    }
    else
    // if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HELP))) {
    //     #if AT_MODE_HELP_SUPPORTED
    //         String plugin;
    //         StringVector findItems;
    //         for(auto strPtr: args.getArgs()) {
    //             String str = strPtr;
    //             str.trim();
    //             str.toLowerCase();
    //             findItems.push_back(str);
    //         }
    //         at_mode_generate_help(output, &findItems);
    //     #else
    //         at_mode_print_help(output);
    //     #endif
    // }
    #if ESP8266 && DEBUG
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPIO))) {
/*
+dumpio=0x700,0x7ff
+dumpio=0x1200,0x1300
+dumpio=GPI
*/
            static constexpr auto kIOBase = 0x60000000U; // std::addressof(ESP8266_REG(0));
            uintptr_t addr = translateAddress(args.toString(0));
            if (addr == ~0U) {
                addr = args.toNumber(0, kIOBase);
            }
            if (addr < kIOBase) {
                addr += kIOBase;
            }
            uintptr_t toAddr = args.toNumber(1, sizeof(uint32_t));
            if (toAddr == 0) {
                toAddr = sizeof(uint32_t);
            }
            if (toAddr < addr) {
                toAddr += addr;
            }
            addr &= 0xffff;
            toAddr &= 0xffff;
            if (addr & 0x3)  {
                args.print(F("address=0x%08x not aligned"), addr);
            }
            else {
                while(addr < toAddr) {
                    auto data = ESP8266_REG(addr);
                    uint8_t len;
                    switch(toAddr - addr) {
                        case 1:
                            data &= 0xff;
                            len = 2;
                            break;
                        case 2:
                            data &= 0xffff;
                            len = 4;
                            break;
                        case 3:
                            data &= 0xffffff;
                            len = 6;
                            break;
                        default:
                            len = 8;
                            break;
                    }
                    Serial.printf_P(PSTR("address=0x%0*.*x data=0x%0*.*x %u %d %s\n"), len, len, addr + kIOBase, len, len, data, data, data, BitsToStr<32, false>(data).c_str() + ((8 - len) * 4));
                    addr += sizeof(uint32_t);
                    delay(1);
                }
            }
        }
        else
    #endif
    #if DEBUG
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPF))) {
            static constexpr size_t kFlashBufferSize = 32;
            uintptr_t addr = translateAddress(args.toString(0));
            if (addr == ~0U) {
                addr = args.toNumber(0, SECTION_FLASH_START_ADDRESS);
            }
            uintptr_t toAddr = args.toNumber(1, 32U);
            if (!toAddr) {
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr = addr + kFlashBufferSize;
            }
            else if (toAddr >= addr) { // length or address?
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr -= SECTION_FLASH_START_ADDRESS;
                if (addr == toAddr) {
                    toAddr += sizeof(4);
                }
            }
            else {
                addr -= SECTION_FLASH_START_ADDRESS;
                toAddr += addr;
            }
            uint8_t buf[kFlashBufferSize];
            std::fill_n(buf, sizeof(buf), 0xff);

            auto &stream = args.getStream();
            uint32_t start = millis();
            while(addr < toAddr) {
                if ((millis() - start) > 5000) {
                    stream.println(F("timeout..."));
                    break;
                }
                uint16_t len = toAddr - addr;
                if (len > sizeof(buf)) {
                    len = sizeof(buf);
                }
                auto result = ESP.flashRead(addr, buf, len);
                if (result) {
                    DumpBinary(stream, DumpBinary::kGroupBytesDefault, sizeof(buf), addr + SECTION_FLASH_START_ADDRESS).dump(buf, len);
                }
                else {
                    stream.printf_P(PSTR("address=0x%08x offset=%u len=%u read error\n"), addr + SECTION_FLASH_START_ADDRESS, addr, len);
                    break;
                }
                addr += len;
                delay(1);
            }
        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FLASH))) {
            // +flash=<e[rase]>,<address>|<r[ead]>,<address>[,<offset=0>,<length=4096>]|w[rite],<address>,<byte1>[,<byte2>[,...]]]>
    /*

    +flash=r,0x405ab000,0,32
    +flash=r,0x405ab000,0,256
    +flash=e,0x405ab000
    +flash=w,0x405ab000,0,1,2,3,4,5,6,7,8
    +flash=r,0x405ab000,0,8


    // EEPROM
    +flash=r,EEPROM,0,1024
    +flash=r,EEPROM,0,128

    +flash=e,EEPROM

    */
            auto cmdStr = args.get(0);
            if (cmdStr) {
                int cmd = stringlist_ifind_P(F("erase,e,read,r,write,w"), cmdStr);
                uintptr_t addr = translateAddress(args.toString(1));
                if (addr == ~0U) {
                    addr = static_cast<uint32_t>(args.toNumber(1, SECTION_FLASH_START_ADDRESS));
                }
                auto offset = static_cast<uint32_t>(args.toNumber(2, 0));
                addr += offset;
                auto length = static_cast<uint32_t>(args.toNumber(3, SPI_FLASH_SEC_SIZE));
                uint16_t sector = (addr - SECTION_FLASH_START_ADDRESS) / SPI_FLASH_SEC_SIZE;
                // recalculate address and offset
                offset = (addr - SECTION_FLASH_START_ADDRESS) - (sector * SPI_FLASH_SEC_SIZE);
                addr = (sector * SPI_FLASH_SEC_SIZE);
                bool rc = true;
                static constexpr size_t kFlashBufferSize = 32;
                switch(cmd) {
                    case 0: // erase
                    case 1: // e
                        args.print(F("erasing sector %u [%08X]"), sector, addr + SECTION_FLASH_START_ADDRESS);
                        if ((rc = ESP.flashEraseSector(sector)) == false) {
                            args.print(F("erase failed"));
                        }
                        break;
                    case 2: // read
                    case 3: // r
                        {
                            args.print(F("reading sector %u address 0x%08x offset %u length %u"), sector, addr + SECTION_FLASH_START_ADDRESS, offset, length);
                            uint32_t start = addr + offset;
                            uint32_t end = start + length;
                            uint8_t buf[kFlashBufferSize];
                            std::fill_n(buf, sizeof(buf), 0xff);
                            while (start < end) {
                                uint8_t len = std::min<size_t>(sizeof(buf), end - start);
                                if ((rc = ESP.flashRead(start, buf, len)) == false) {
                                    args.print(F("read error address=0x%08x length=%u"), start, length);
                                    break;
                                }
                                DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, sizeof(buf), start + SECTION_FLASH_START_ADDRESS).dump(buf, len);
                                start += len;
                                delay(1);
                            }
                        }
                        break;
                    case 4: // write
                    case 5: // w
                        {
                            if (args.size() <= 3) {
                                args.print(F("no data to write"));
                            }
                            else {
                                length = args.size() - 3;
                                args.print(F("writing sector %u (0x%08x) offset %u length %u"), sector, addr + SECTION_FLASH_START_ADDRESS, offset, length);
                                uint16_t position = 0;
                                auto &stream = args.getStream();
                                auto data = std::unique_ptr<uint8_t[]>(new uint8_t[kFlashBufferSize]());
                                auto compare = std::unique_ptr<uint8_t[]>(new uint8_t[kFlashBufferSize]());
                                uint32_t start = addr + offset;
                                if (data && compare) {
                                    rc = 0;
                                    auto ptr = data.get();
                                    for(uint16_t i = 3; i < args.size(); i++) {
                                        if (i == 3) {
                                            stream.printf_P(PSTR("[%08X] "), start + SECTION_FLASH_START_ADDRESS);
                                        }
                                        auto value = static_cast<uint8_t>(args.toNumber(i, 0xff));
                                        *ptr++ = value;
                                        stream.printf_P(PSTR("%02x "), value);
                                        // once the buffer is full, write and verify
                                        if (++position % kFlashBufferSize == 0) {
                                            stream.println();
                                            if (!_writeAndVerifyFlash(start, data.get(), kFlashBufferSize, compare.get(), args)) {
                                                position = 0;
                                                break;
                                            }
                                            delay(1);
                                            // reset buffer ptr
                                            ptr = data.get();
                                            // move address ahead
                                            start += 32;
                                            if (i < args.size() - 1) {
                                                stream.printf_P(PSTR("[%08X] "), start + SECTION_FLASH_START_ADDRESS);
                                            }
                                        }
                                    }

                                    // data left?
                                    auto rest = position % kFlashBufferSize;
                                    if (rest != 0) {
                                        stream.println();
                                        _writeAndVerifyFlash(start, data.get(), rest, compare.get(), args);
                                    }
                                }
                                else {
                                    args.print(F("failed to allocate %u bytes"), length);
                                }
                            }
                        }
                        break;
                    default:
                        args.print(F("invalid command: %s"), cmdStr);
                        break;
                }
            }
            else {
                args.print(F("invalid command"));
            }

        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(METRICS))) {
            #if 1
                args.print(F("Device name: %s"), System::Device::getName());
                #if ESP32
                    args.print(F("Framework Arduino ESP32 " ARDUINO_ESP32_RELEASE));
                    args.print(F("ESP-IDF version %s-dev version"), esp_get_idf_version());
                #elif ESP8266
                    args.print(F("Framework Arduino ESP8266 " ARDUINO_ESP8266_RELEASE), ARDUINO_ESP8266_GIT_VER);
                #endif
                // args.print(F("Git describe: " KFCFW_GIT_DESCRIBE));
                #if defined(HAVE_GDBSTUB) && HAVE_GDBSTUB
                {
                    String options;
                    #if GDBSTUB_USE_OWN_STACK
                    options += F("USE_OWN_STACK ");
                    #endif
                    #if GDBSTUB_BREAK_ON_EXCEPTION
                    options += F("BREAK_ON_EXCEPTION ");
                    #endif
                    #if GDBSTUB_CTRLC_BREAK
                    options += F("CTRLC_BREAK ");
                    #endif
                    #if GDBSTUB_REDIRECT_CONSOLE_OUTPUT
                    options += F("REDIRECT_CONSOLE_OUTPUT ");
                    #endif
                    #if GDBSTUB_BREAK_ON_INIT
                    options += F("BREAK_ON_INIT ");
                    #endif

                    args.print(F("GDBStub: %s"), options.trim().c_str());
                }
                #endif
                args.print(F("Uptime: %u seconds / %s"), getSystemUptime(), formatTime(getSystemUptime(), true).c_str());
                args.print(F("Free heap/fragmentation: %u / %u"), ESP.getFreeHeap(), ESP.getHeapFragmentation());
                #if ARDUINO_ESP8266_MAJOR >= 3
                    #ifdef UMM_HEAP_IRAM
                        {
                            HeapSelectIram ephemeral;
                            args.print(F("Free IRAM: %u"), ESP.getFreeHeap());
                        }
                    #endif
                    #if (UMM_NUM_HEAPS != 1)
                        {
                            HeapSelectDram ephemeral;
                            args.print(F("Free DRAM: %u"), ESP.getFreeHeap());
                        }
                    #endif
                #endif

                #if ESP8266
                    args.print(F("irom0.text: 0x%08x-0x%08x"), SECTION_IROM0_TEXT_START_ADDRESS, SECTION_IROM0_TEXT_END_ADDRESS);
                #endif
                args.print(F("EEPROM: 0x%x/%u"), SECTION_EEPROM_START_ADDRESS, SECTION_EEPROM_END_ADDRESS - SECTION_EEPROM_START_ADDRESS + 4096);
                args.print(F("SaveCrash: 0x%x/%u"), SECTION_SAVECRASH_START_ADDRESS, SECTION_SAVECRASH_END_ADDRESS - SECTION_SAVECRASH_START_ADDRESS + 4096);
                args.print(F("NVS: 0x%x/%u"), SECTION_NVS_START_ADDRESS, SECTION_NVS_END_ADDRESS - SECTION_NVS_START_ADDRESS + 4096);
                args.print(F("NVS2: 0x%x/%u"), SECTION_NVS2_START_ADDRESS, SECTION_NVS2_END_ADDRESS - SECTION_NVS2_START_ADDRESS + 4096);
                #if ESP8266
                    args.print(F("DRAM: 0x%08x-0x%08x/%u"), SECTION_DRAM_START_ADDRESS, SECTION_DRAM_END_ADDRESS, SECTION_DRAM_END_ADDRESS - SECTION_DRAM_START_ADDRESS);
                    args.print(F("HEAP: 0x%08x-0x%08x/%u"), SECTION_HEAP_START_ADDRESS, SECTION_HEAP_END_ADDRESS, SECTION_HEAP_END_ADDRESS - SECTION_HEAP_START_ADDRESS);
                    {
                        int stackAddress = 0;
                        args.print(F("Stack: 0x%08x-0x%08x/%u"), (uint32_t)&stackAddress, SECTION_STACK_END_ADDRESS, SECTION_STACK_END_ADDRESS - (uint32_t)&stackAddress);
                    }
                #endif
                args.print(F("CPU frequency: %uMHz %u core(s)"), ESP.getCpuFreqMHz(), ESPGetChipCores());
                #if ESP32
                    args.print(F("Chip model %s (%s)"), KFCFWConfiguration::getChipModel(), ESP.getChipModel());
                    args.print(F("Flash size / mode: %s / %s"), formatBytes(ESP.getFlashChipSize()).c_str(), ESPGetFlashChipSpeedAndModeStr().c_str());
                #elif ESP8266
                    args.print(F("Chip model %s"), KFCFWConfiguration::getChipModel());
                    args.print(F("Flash size / vendor / mode: %s / %02x / %s"), formatBytes(ESP.getFlashChipRealSize()).c_str(), ESP.getFlashChipVendorId(), ESPGetFlashChipSpeedAndModeStr().c_str());
                    args.print(F("SDK / Core: %s / %s"), ESP.getSdkVersion(), ESP.getFullVersion().c_str());
                    args.print(F("Boot mode: %u, %u"), ESP.getBootVersion(), ESP.getBootMode());
                #endif
                args.print(F("Firmware size: %s"), formatBytes(ESP.getSketchSize()).c_str());
                args.print(F("Version (uint32): %s (0x%08x)"), SaveCrash::Data::FirmwareVersion().toString().c_str(), SaveCrash::Data::FirmwareVersion().__version);
                auto version = System::Device::getConfig().config_version;
                args.print(F("Config version 0x%08x, %s"), version, SaveCrash::Data::FirmwareVersion(version).toString().c_str());
                args.print(F("MD5 hash: %s"), SaveCrash::Data().getMD5().c_str());
                args.print(F("WiFiCallbacks: size=%u count=%u"), sizeof(WiFiCallbacks::Entry), WiFiCallbacks::getVector().size());
                args.print(F("LoopFunctions: size=%u count=%u"), sizeof(LoopFunctions::Entry), LoopFunctions::getVector().size());

                #if PIN_MONITOR
                    PrintString tmp;
                    PinMonitor::pinMonitor.printStatus(tmp);
                    tmp.replace(F(HTML_S(br)), "\n");
                    tmp.rtrim('\n');
                    args.print(tmp);
                #endif

                args.print(F("Firmware MD5: %s"), System::Firmware::getFirmwareMD5());
            #endif
        }
        else
    #endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RST))) {
        bool safeMode = false;
        if (args.equals(0, 's')) {
            safeMode = true;
            args.print(F("Software reset, safe mode enabled..."));
        }
        else {
            args.print(F("Software reset..."));
        }
        LoopFunctions::callOnce([safeMode]() {
            config.restartDevice(safeMode);
        });
    }
    // else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CMDS))) {
    //     output.print(F("+CMDS="));
    //     #if AT_MODE_HELP_SUPPORTED
    //         at_mode_print_command_string(output, ',');
    //     #endif
    //     output.println();
    // }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOAD))) {
        config.read();
        args.ok();
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(STORE))) {
        config.write();
        args.ok();
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(IMPORT))) {
        if (args.requireArgs(1)) {
            auto res = false;
            auto filename = args.get(0);
            if (!strcmp_P(filename, PSTR("set_dirty"))) {
                config.setConfigDirty(true);
                args.print(F("Configuration marked dirty"));
            }
            else {
                auto file = KFCFS.open(filename, fs::FileOpenMode::read);
                if (file) {
                    args.print(filename);
                    Configuration::Handle_t *handlesPtr = nullptr;
                    Configuration::Handle_t handles[kCommandParserMaxArgs + 1];
                    auto iterator = args.begin();
                    if (++iterator != args.end()) {
                        output.print(F(": "));
                        handlesPtr = &handles[0];
                        auto count = 0;
                        while(iterator != args.end() && count < kCommandParserMaxArgs) {
                            handles[count++] = (uint16_t)strtoul(*iterator, nullptr, 0); // auto detect base
                            handles[count] = 0;
                            output.printf_P(PSTR("0x%04x "), handles[count - 1]);
                            ++iterator;
                        }
                    }
                    output.println();
                    res = config.importJson(file, handlesPtr);
                }
                if (res) {
                    args.ok();
                    config.write();
                    config.setConfigDirty(true);
                } else {
                    args.print(F("Failed to import: %s"), filename);
                }
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FACTORY))) {
        config.restoreFactorySettings();
        args.ok();
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSR))) {
        config.restoreFactorySettings();
        config.write();
        args.ok();
        LoopFunctions::callOnce([]() {
            config.restartDevice(false);
        });
    }
    #if DEBUG
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ATMODE))) {
            if (args.requireArgs(1, 1)) {
                if (args.isTrue(0)) {
                    enable_at_mode(&output);
                }
                else {
                    disable_at_mode(&output);
                }
            }
        }
    #endif
    #if __LED_BUILTIN_WS2812_NUM_LEDS && DEBUG
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(NEOPX))) {
            // +neopx=16,3,25,0,0
            if (args.requireArgs(3, 5)) {
                auto pin = static_cast<uint8_t>(args.toInt(0));
                auto num = static_cast<uint16_t>(args.toInt(1));
                auto red = static_cast<uint8_t>(args.toInt(2));
                auto green = static_cast<uint8_t>(args.toInt(3, red));
                auto blue = static_cast<uint8_t>(args.toInt(4, red));
                if (num == 0) {
                    args.print(F("pin=%u num=%u - invalid number"), pin, num);
                }
                else {
                    uint32_t color = (red << 16) | (green << 8) | blue;
                    args.print(F("pin=%u num=%u color=#%06x"), pin, num, color);
                    if (ledTimer) {
                        delete ledTimer;
                        ledTimer = nullptr;
                    }
                    digitalWrite(pin, LOW);
                    pinMode(pin, OUTPUT);
                    #if HAVE_FASTLED
                        fill_solid(WS2812LEDTimer::_pixels, sizeof(WS2812LEDTimer::_pixels) / 3, CRGB(color));
                        FastLED.show();
                    #else
                        WS2812LEDTimer::_pixels.fill(color);
                        WS2812LEDTimer::_pixels.show();
                    #endif
                }
            }
        }
    #endif
    #if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LED))) {
            if (args.requireArgs(1, 4)) {
                BlinkLEDTimer::BlinkType type = BlinkLEDTimer::BlinkType::INVALID;
                String mode = args.toString(0);
                auto delay = static_cast<uint16_t>(args.toInt(2, 50));
                auto pin = static_cast<uint8_t>(args.toInt(3, __LED_BUILTIN));
                if (mode.startsWith(F("pat"))) {
                    #if BUILTIN_LED_NEOPIXEL
                        if (pin == BlinkLEDTimer::NEOPIXEL_PIN) {
                            args.print(F("Pattern not supported with NeoPixel"));
                        }
                        else
                    #endif
                    {
                        auto patternStr = args.toString(1);
                        auto pattern = BlinkLEDTimer::Bitset();
                        pattern.fromString(patternStr);
                        args.print(F("pattern %s delay %u"), pattern.toString().c_str(), delay);
                        BlinkLEDTimer::setPattern(pin, delay, std::move(pattern));
                        //+led=pattern,111111111111111111111111111111111111111111111111111111,100
                        //+led=pattern,1010,100
                    }
                }
                else {
                    // +LED=slow,200000,1000,10
                    // +LED=slow,000000,1000,16
                    // pwm 16 output 0
                    // pwm 15 output 0
                    // led off
                    auto color = static_cast<int32_t>(args.toNumber(1, 0xff00ff, 16));
                    if (__LED_BUILTIN == pin && !BlinkLEDTimer::isPinValid(pin)) {
                        args.print(F("Invalid PIN"));
                    }
                    else {
                        if (mode.equalsIgnoreCase(F("slow"))) {
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SLOW, color);
                        }
                        else if (mode.equalsIgnoreCase(F("fast"))) {
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::FAST, color);
                        }
                        else if (mode.equalsIgnoreCase(F("flicker"))) {
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::FLICKER, color);
                        }
                        else if (mode.equalsIgnoreCase(F("solid"))) {
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SOLID, color);
                        }
                        else if (mode.equalsIgnoreCase(F("sos"))) {
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::SOS, color);
                        }
                        else {
                            mode = F("OFF");
                            color = 0;
                            BlinkLEDTimer::setBlink(pin, type = BlinkLEDTimer::BlinkType::OFF);
                        }
                        args.print(F("LED pin=%u mode=%s type=%u color=0x%06x"), pin, mode.c_str(), type, color);
                    }
                }
            }
        }
    #endif
    #if HAVE_I2CSCANNER
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CSCAN))) {
            auto startAddress = args.toIntMinMax<uint8_t>(0, 1, 255, 1);
            auto endAddress = args.toIntMinMax<uint8_t>(1, startAddress, 255, 127);
            auto sda = args.toIntMinMax<uint8_t>(2, 0, 16, KFC_TWOWIRE_SDA);
            auto scl = args.toIntMinMax<uint8_t>(3, 0, 16, KFC_TWOWIRE_SCL);
            if (args.has(F("noinit")) || args.has(F("no-init"))) {
                sda = scl = 0xff;
            }
            if (args.has(F("any"))) {
                scanPorts(args.getStream(), startAddress, endAddress);
            }
            else {
                scanI2C(args.getStream(), sda, scl, startAddress, endAddress);
            }
        }
    #endif
    #ifndef DISABLE_TWO_WIRE
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CS))) {
        auto sda = args.toIntMinMax<uint8_t>(0, 0, NUM_DIGITAL_PINS, KFC_TWOWIRE_SDA);
        auto scl = args.toIntMinMax<uint8_t>(1, 0, NUM_DIGITAL_PINS, KFC_TWOWIRE_SCL);
        uint32_t speed = args.toInt(2, KFC_TWOWIRE_CLOCK_SPEED);
        uint32_t stretch = args.toInt(3, KFC_TWOWIRE_CLOCK_STRETCH);
        bool stop = args.has(F("stop"));
        if (isFlashInterfacePin(sda) || isFlashInterfacePin(scl)) {
            args.print(F("Pins 6, 7, 8, 9, 10 and 11 cannot be used"));
        }
        else if (stop) {
            pinMode(sda, INPUT);
            pinMode(scl, INPUT);
            Wire.begin(255, 255);
            args.print(F("I2C stopped"));
        }
        else {
            config.initTwoWire(true);
            Wire.begin(sda, scl);
            Wire.setClockStretchLimit(stretch);
            Wire.setClock(speed);
            args.print(F("I2C started on %u:%u (sda:scl), speed %u, clock stretch %u"), sda, scl, speed, stretch);
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CTM))) {
        if (args.requireArgs(1)) {
            uint8_t address = args.toNumber(0, 0x48);
            uint8_t written = 0;
            Wire.beginTransmission(address);
            for(uint8_t i = 1; i < args.size(); i++) {
                written += Wire.write(args.toNumber(i, 0xff));
            }
            uint8_t error;
            if ((error = Wire.endTransmission(true)) == 0) {
                args.print(F("slave 0x%02X: transmitted %u bytes"), address, written);
            }
            else {
                args.print(F("slave 0x%02X: transmitted %u bytes, error %u"), address, written, error);
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CRQ))) {
        if (args.requireArgs(1, 2)) {
            uint8_t address = args.toNumber(0, 0x48);
            uint8_t length = args.toNumber(1, 0);
            if (Wire.requestFrom(address, length) == length) {
                auto pbuf = std::unique_ptr<uint8_t[]>(new uint8_t[length + 1]());
                auto buf = pbuf.get();
                if (!buf) {
                    args.print(F("failed to allocate memory. %u bytes"), length + 1);
                }
                else {
                    auto read = Wire.readBytes(buf, length);
                    args.print(F("slave 0x%02X: requested %u byte. data:"), address, read);
                    DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, 16).dump(buf, read);
                }
            }
            else {
                args.print(F("slave 0x%02X: requesting data failed, length=%u"), address, length);
            }
        }
    }
    else if (args.isCommand(F("I2CT")) || args.isCommand(F("I2CA")) || args.isCommand(F("I2CR"))) {
        // ignore SerialTwoWire communication
    }
    #endif
    #if ENABLE_ARDUINO_OTA
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(AOTA))) {
            auto &plugin = WebServer::Plugin::getInstance();
            if (args.equalsIgnoreCase(0, F("start"))) {
                args.print(F("starting ArduinoOTA..."));
                plugin.ArduinoOTAbegin();
            }
            #if 1
                else if (args.equalsIgnoreCase(0, F("stop"))) {
                    args.print(F("stopping ArduinoOTA..."));
                    plugin.ArduinoOTAend();
                }
                else {
                    args.print(F("ArduinoOTA status:"));
                    plugin.ArduinoOTADumpInfo(args.getStream());
                }
            #endif
        }
    #endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WIFI))) {

        if (args.requireArgs(1)) {
            auto cmd = static_cast<WiFiCommandsType>(stringlist_find_P_P(PSTR(WIFI_COMMANDS), args.get(0), '|'));
            switch(cmd) {
                case WiFiCommandsType::RESET:
                    config.reconfigureWiFi(F("reconfiguring WiFi adapter"));
                    break;
                case WiFiCommandsType::ST_ON:
                    args.print(F("enabling station mode"));
                    WiFi.enableSTA(true);
                    WiFi.reconnect();
                    break;
                case WiFiCommandsType::ST_OFF:
                    args.print(F("disabling station mode"));
                    WiFi.enableSTA(false);
                    break;
                case WiFiCommandsType::ST_LIST: {
                        auto network = Network::Settings::getConfig();
                        for(uint8_t i = 0; i < Network::WiFi::kNumStations; i++) {
                            at_mode_print_WiFi_info(args, i, network.stations[i], args.isTrue(1));
                        }
                    }
                    break;
                case WiFiCommandsType::ST_CFG: {
                        if (args.size() < 3) {
                            args.print(F("+WIFI=cfg,<connection=%u-%u>,<enable=1|disable=0|remove>,<SSID>,<password>[,<DHCP>|<IP>,<subnet>,<gateway>[,<DNS1|global>,<DNS2|global>]"),
                                Network::WiFi::StationConfigType::CFG_0, Network::WiFi::StationConfigType::CFG_LAST
                            );
                            auto num = args.toIntMinMax<uint8_t>(1, 0, Network::WiFi::kNumStations - 1, config.getWiFiConfigurationNum());
                            auto &network = Network::Settings::getWriteableConfig();
                            auto &cfg = network.stations[num];
                            if (args.startsWithIgnoreCase(2, F("remove"))) {
                                args.print(F("removed SSID %s"), Network::WiFi::getSSID(num));
                                cfg.enabled = false;
                                Network::WiFi::setSSID(num, emptyString);
                                Network::WiFi::setPassword(num, emptyString);
                            }
                            else {
                                auto SSID = args.toString(3);
                                auto password = args.toString(4);
                                auto ip = args.toString(5);
                                cfg.enabled = args.isTrue(2);
                                if (ip.startsWithIgnoreCase(F("dhcp"))) {
                                    cfg.dhcp = true;
                                }
                                else {
                                    auto subnet = args.toString(6);
                                    auto gateway = args.toString(7);
                                    auto dns1 = args.toString(8);
                                    auto dns2 = args.toString(9);
                                    cfg.dhcp = false;
                                    cfg.local_ip = IPAddress().fromString(ip);
                                    cfg.subnet = IPAddress().fromString(subnet);
                                    cfg.gateway = IPAddress().fromString(gateway);
                                    cfg.dns1 = dns1.startsWithIgnoreCase(F("glob")) ? Network::Settings::kGlobalDNS : IPAddress().fromString(dns1);
                                    cfg.dns2 = dns2.startsWithIgnoreCase(F("glob")) ? Network::Settings::kGlobalDNS : IPAddress().fromString(dns2);
                                }
                                Network::WiFi::setSSID(num, SSID);
                                Network::WiFi::setPassword(num, password);
                                network.activeNetwork = num;
                                config.write();

                                at_mode_print_WiFi_info(args, num, network.stations[num]);
                                config.reconfigureWiFi(F("reconfiguring WiFi adapter"), static_cast<Network::WiFi::StationConfigType>(network.activeNetwork));
                            }
                        }
                    }
                    break;
                case WiFiCommandsType::AP_ON:
                    args.print(F("enabling AP mode"));
                    WiFi.enableAP(true);
                    break;
                case WiFiCommandsType::AP_OFF:
                    args.print(F("disabling AP mode"));
                    WiFi.enableAP(false);
                    break;
                case WiFiCommandsType::AP_STBY: {
                        args.print(F("disabling AP mode (stand-by is enabled)"));
                        WiFi.enableAP(false);
                        auto &flags = System::Flags::getWriteableConfig();
                        flags.is_softap_standby_mode_enabled = true;
                        flags.is_softap_enabled = false;
                        config.write();
                        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, KFCFWConfiguration::apStandbyModeHandler);
                    }
                    break;
                case WiFiCommandsType::DIAG:
                    config.printDiag(args.getStream(), F("+WIFI: "));
                    break;
                case WiFiCommandsType::AVAIL_ST_LIST: {
                        for(const auto &station: KFCConfigurationClasses::Network::WiFi::getStations(nullptr)) {
                            args.print(F("%02u: %-32.32s %03u %s"), station._id, station._SSID.c_str(), station._priority, mac2String(station._bssid).c_str());
                        }
                    }
                    break;
                case WiFiCommandsType::NEXT:
                    config.setWiFiErrors(0xff - 2);
                    config.registerWiFiError();
                    args.print(F("switching WiFi network"));
                    config.reconfigureWiFi(nullptr);
                    break;
            }
        }


            // // auto arg0 = args.toString(0);
            // // if (arg0.endsWithIgnoreCase(F("wimo"))) {
            // //     args.print(F("Setting WiFi mode and restarting device..."));
            // //     System::Flags::getWriteableConfig().setWifiMode(args.toUint8(1));
            // //     config.write();
            // //     config.restartDevice();
            // // }
            // else if (arg0.startsWithIgnoreCase(F("clear"))) {
            //     #if ESP8266
            //         station_config wifiConfig;
            //         auto result = wifi_station_set_config(&wifiConfig);
            //         args.print(F("clearing default config from flash... %s"), result ? PSTR("success") : PSTR("failure"));
            //     #endif
            //     config.reconfigureWiFi(F("reconfiguring WiFi adapter"));
            //     config.printDiag(args.getStream(), F("+WIFI: "));
            // }
            // else if (arg0.startsWithIgnoreCase(F("diag"))) {

            // }
            // else if (arg0.equalsIgnoreCase(F("reset"))) {
            //     config.reconfigureWiFi(F("reconfiguring WiFi adapter"));
            //     config.printDiag(args.getStream(), F("+WIFI: "));
            // }
        }

        // auto flags = System::Flags::getConfig();
        // args.print(F("station mode %s, DHCP %s, SSID %s, connected %s, IP %s"),
        //     (WiFi.getMode() & WIFI_STA) ? SPGM(on) : SPGM(off),
        //     flags.is_station_mode_dhcp_enabled ? SPGM(on) : SPGM(off),
        //     WiFi.SSID().c_str(),
        //     WiFi.isConnected() ? SPGM(yes) : SPGM(no),
        //     WiFi.localIP().toString().c_str()
        // );
        // args.print(F("AP mode %s, DHCP %s, SSID %s, clients connected %u, IP %s"),
        //     (flags.is_softap_enabled) ? ((flags.is_softap_standby_mode_enabled) ? ((WiFi.getMode() & WIFI_AP) ? SPGM(on) : PSTR("stand-by")) : SPGM(on)) : SPGM(off),
        //     flags.is_softap_dhcpd_enabled ? SPGM(on) : SPGM(off),
        //     Network::WiFi::getSoftApSSID(),
        //     WiFi.softAPgetStationNum(),
        //     Network::SoftAP::getConfig().getAddress().toString().c_str()
        // );
    // }
    #if RTC_SUPPORT
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTC))) {
            if (args.empty()) {
                args.print(F("Time=" TIME_T_FMT ", rtc=%u, lostPower=%u"), time(nullptr), config.getRTC(), config.rtcLostPower());
                output.print(F("+RTC: "));
                config.printRTCStatus(output);
                output.println();
            }
            else {
                args.print(F("Set=%u, rtc=%u"), config.setRTC(time(nullptr)), config.getRTC());
            }
        }
    #endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(REM))) {
        // ignore comment
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DLY))) {
        auto delayTime = args.toMillis(0, 1, 3600 * 1000, 250, F("ms"));
        args.print(F("%ums"), delayTime);
        delay(delayTime);

        // if (commandQueue == nullptr && delayTime < 5) { // queue not active and delay very short delays
        //     args.print(F("%ums"), delayTime);
        //     delay(delayTime);
        // }
        // else {
        //     if (!commandQueue) { // create new queue and delay next command
        //         commandQueue = new CommandQueue(output);
        //     }
        //     commandQueue->setDelay(delayTime);
        // }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RM))) {
        if (args.requireArgs(1, 1)) {
            auto filename = args.get(0);
            auto result = KFCFS.remove(filename);
            args.print(F("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RN))) {
        if (args.requireArgs(2, 2)) {
            auto filename = args.get(0);
            auto newFilename = args.get(1);
            auto result = KFCFS.rename(filename, newFilename);
            args.print(F("%s => %s: %s"), filename, newFilename, result ? PSTR("success") : PSTR("failure"));
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LS))) {
        auto dir = ListDir(args.toString(0), !args.isTrue(2, true), args.isFalse(1, true));
        while(dir.next()) {
            output.print(F("+LS: "));
            if (dir.isFile()) {
                output.printf_P(PSTR("%8.8s "), formatBytes(dir.fileSize()).c_str());
            }
            else {
                output.print(F("[...]    "));
            }
            output.println(dir.fileName());
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LSR))) {
        auto dir = KFCFS_openDir(args.toString(0));
        while(dir.next()) {
            output.print(F("+LS: "));
            if (dir.isDirectory()) {
                output.print(F("[...]    "));
            }
            else {
                output.printf_P(PSTR("%8.8s "), formatBytes(dir.fileSize()).c_str());
            }
            output.println(dir.fileName());
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CAT))) {
        if (args.requireArgs(1, 1)) {
            StreamOutput::Cat::dump(args.toString(0), output, StreamOutput::Cat::kPrintInfo|StreamOutput::Cat::kPrintCrLfAsText);
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PLG))) {
        if (args.requireArgs(1, 2)) {
            auto cmds = PSTR("list|start|stop|add-blacklist|add|remove");
            int cmd = stringlist_find_P_P(cmds, args.get(0), '|');
            __DBG_printf("cmd=%d arg0=%s cmds=%s", cmd, args.get(0), cmds);
            if (cmd == 0) {
                PluginComponents::RegisterEx::getInstance().dumpList(output);
                args.print(F("Blacklist=%s"), PluginComponent::getBlacklist());
            }
            else if (args.requireArgs(2, 2)) {
                PluginComponent *plugin = nullptr;
                plugin = PluginComponent::findPlugin(FPSTR(args.get(1)), false);
                if (!plugin) {
                    args.print(F("Cannot find plugin '%s'"), args.get(1));
                }
                else {
                    switch(cmd) {
                        case 1: // start
                            if (plugin->getSetupTime() == 0) {
                                args.print(F("Calling %s.setup()"), plugin->getName_P());
                                plugin->setSetupTime();
                                PluginComponents::DependenciesPtr deps(new PluginComponents::Dependencies());
                                plugin->setup(PluginComponent::SetupModeType::DEFAULT, deps);
                            }
                            else {
                                args.print(F("%s already running"), plugin->getName_P());
                            }
                            break;
                        case 2: // stop
                            if (plugin->getSetupTime() != 0) {
                                args.print(F("Calling %s.shutdown()"), plugin->getName_P());
                                plugin->shutdown();
                                plugin->clearSetupTime();
                            }
                            else {
                                args.print(F("%s not running"), plugin->getName_P());
                            }
                            break;
                        case 3:     // add-blacklist
                        case 4:     // add
                        case 5:     // remove
                            {
                                bool flag;
                                if (cmd == 5) {
                                    flag = PluginComponent::removeFromBlacklist(plugin->getName());
                                }
                                else {
                                    flag = PluginComponent::addToBlacklist(plugin->getName());
                                }
                                if (flag) {
                                    config.write();
                                }
                                args.printf_P("Blacklist=%s action=%s", PluginComponent::getBlacklist(), flag ? SPGM(success, "success") : SPGM(failure));
                            } break;
                        default:
                            args.print(F("expected <%s>"), cmds);
                            break;
                    }
                }
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
        auto &streams = *serialHandler.getStreams();
        auto &clients = serialHandler.getClients();
        args.print(F("outputs=%u clients=%u"), streams.size(), clients.size());
        for(const auto stream: streams) {
            args.print(F("output %p"), stream);
        }
        args.print(F("input %p"), serialHandler.getInput());
        for(const auto &client: clients) {
            args.print(F("client %p: events: %02x"), client.get(), client->getEvents());
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
        // Mappings::getInstance().dump(output);
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
        if (args.requireArgs(0, 1)) {
            auto interval = args.toMillis(0, 0, 3600 * 1000, 0, String('s'));
            if (interval < 250) {
                if (displayTimer) {
                    displayTimer->remove();
                    args.print(F("Interval disabled"));
                }
                    print_heap();
                    Serial.println();
            }
            else {
                if (!displayTimer) {
                    new DisplayTimer();
                }
                if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI))) {
                    displayTimer->setType(DisplayTimer::DisplayType::RSSI, Event::milliseconds(interval));
                }
                else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                    displayTimer->setType(DisplayTimer::DisplayType::GPIO, Event::milliseconds(interval));
                }
                else {
                    displayTimer->setType(DisplayTimer::DisplayType::HEAP, Event::milliseconds(interval));
                }
                args.print(F("Interval set to %ums"), interval);
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PWM))) {
        if (args.requireArgs(2, 7)) {
            auto pin = args.toUint8(0);
            if (args.equalsIgnoreCase(1, F("waveform"))) {
                #if ESP8266
                    if (pin > 16) {
                        args.print(F("%u does not support waveform"), pin);
                    }
                    else {
                        uint32_t timeHighUS = args.toInt(2, ~0U);
                        uint32_t timeLowUS = args.toInt(3, ~0U);
                        uint32_t runTimeUS = args.toInt(4, 0);
                        uint32_t increment = args.toInt(5, 0);
                        auto delayTime = args.toUint32(6);
                        if (timeHighUS == ~0U || timeLowUS == ~0U) {
                            digitalWrite(pin, LOW);
                            #if ESP8266
                                stopWaveform(pin);
                            #endif
                            pinMode(pin, INPUT);
                            args.print(F("usage: <pin>,waveform,<high-time cycles>,<low-time cycles>[,<run-time cycles|0=unlimited>,<increment>,<delay ms>]"));
                        }
                        else {
                            digitalWrite(pin, LOW);
                            pinMode(pin, OUTPUT);
                            if (increment) {
                                args.print(F("pin=%u high=%u low=%u runtime=%u increment=%u delay=%u"), pin, timeHighUS, timeLowUS, runTimeUS, increment, delayTime);
                                uint32_t start = 0;
                                _Scheduler.add(Event::milliseconds(delayTime), true, [args, delayTime, pin, timeHighUS, timeLowUS, runTimeUS, increment, start](Event::CallbackTimerPtr timer) mutable {
                                    start += increment;
                                    if (start >= timeHighUS) {
                                        start = timeHighUS;
                                        timer->disarm();
                                    }
                                    startWaveformClockCycles(pin, start, timeLowUS, runTimeUS);
                                    if (delayTime > 20) {
                                        args.print(F("pin=%u high=%u low=%u runtime=%u"), pin, start, timeLowUS, runTimeUS);
                                    }
                                }, Event::PriorityType::TIMER);

                            } else {
                                args.print(F("pin=%u high=%u low=%u runtime=%u"), pin, timeHighUS, timeLowUS, runTimeUS);
                                startWaveformClockCycles(pin, timeHighUS, timeLowUS, runTimeUS);
                            }
                        }
                    }
                #else
                    args.print(F("ESP32 does not support waveform"));
                #endif
            }
            else if (args.equalsIgnoreCase(1, F("input"))) {
                digitalWrite(pin, LOW);
                pinMode(pin, INPUT);
                args.print(F("set pin=%u to INPUT"), pin);
            }
            else if (args.equalsIgnoreCase(1, F("input_pullup"))) {
                digitalWrite(pin, LOW);
                #if ESP8266
                    pinMode(pin, pin == 16 ? INPUT_PULLDOWN_16 : INPUT_PULLUP);
                    args.print(F("set pin=%u to %s"), pin, pin == 16 ? PSTR("INPUT_PULLDOWN_16") : PSTR("INPUT_PULLUP"));
                #else
                    pinMode(pin, INPUT_PULLUP);
                    args.print(F("set pin=%u to %s"), pin, PSTR("INPUT_PULLUP"));
                #endif
            }
            else {

                auto level = (uint16_t)args.toIntMinMax(1, 0, PWMRANGE, 0);
                if (level == 0) {
                    if (args.isAnyMatchIgnoreCase(1, F("h|hi|high"))) {
                        level = PWMRANGE;
                    }
                    else if (args.isAnyMatchIgnoreCase(1, F("l|lo|low"))) {
                        level = 0;
                    }
                }
                auto freq = (uint16_t)args.toIntMinMax(2, 100, 40000, 1000);
                auto duration = (uint16_t)args.toMillis(3);
                String durationStr;
                if (duration > 0 && duration < 10) {
                    duration = 10;
                }

                auto type = PSTR("digitalWrite");

                pinMode(pin, OUTPUT);
                #if defined(ESP8266)
                    analogWriteFreq(freq);
                    analogWriteRange(PWMRANGE);
                #else
                    freq = 0;
                #endif
                if (level == 0)  {
                    digitalWrite(pin, LOW);
                    freq = 0;
                }
                else if (level >= PWMRANGE - 1) {
                    digitalWrite(pin, HIGH);
                    level = 1;
                    freq = 0;
                }
                else {
                    type = PSTR("analogWrite");
                    analogWrite(pin, level);
                }
                if (duration) {
                    durationStr = PrintString(F(" for %ums"), duration);
                }
                if (freq == 0) {
                    args.print(F("%s(%u, %u)%s"), type, pin, level, durationStr.c_str());
                }
                else {
                    float cycle = (1000000 / (float)freq);
                    float dc = cycle * (level / (float)PWMRANGE);
                    args.print(F("%s(%u, %u) duty cycle=%.2f cycle=%.2fµs f=%uHz%s"), type, pin, level, dc, cycle, freq, durationStr.c_str());
                }
                if (duration) {
                    auto &stream = args.getStream();
                    _Scheduler.add(duration, false, [pin, &stream](Event::CallbackTimerPtr) mutable {
                        stream.printf_P(PSTR("+PWM: digitalWrite(%u, 0)\n"), pin);
                        digitalWrite(pin, LOW);
                    }, Event::PriorityType::TIMER);
                }
            }
        }
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ADC))) {
        if (args.requireArgs(1)) {
            if (args.equalsIgnoreCase(0, F("websocket"))) {

                // websocket,<client_id>,<read interval/microseconds>,<duration/ms>[,<packet size=1024>]
                // +ADC=websocket,0x3fff595c,2500,10,1024
                // +ADC=websocket,0,25000,5000,512
                // +ADC=websocket,0,500,5000,1024
                // +ADC=websocket,0x3fff2dcc,500,10000,1536

                uint32_t clientId = args.toNumber(1, 0, 16);
                auto interval = args.toIntMinMax(2, 250U, ~0U, 0xffffffffU);                            // 0.25ms up to 4000 seconds
                auto duration = args.toIntMinMax(3, 1U, (1U << 22), 10000U);                            // 1ms - 4194 seconds
                auto packetSize = args.toIntMinMax(4, 64U, TCP_SND_BUF - 64U, TCP_SND_BUF - 64U);       // ESP8266: TCP_SND_BUF = 536 * 2 = max. 1072 - overhead for the web socket
                AsyncWebSocketClient *client = Http2Serial::getClientById(reinterpret_cast<AsyncWebSocketClient *>(clientId));
                if (client) {
                    at_mode_adc_delete_object();
                    atModeADC = new AtModeADCWebSocket();
                    if (!atModeADC) {
                        __DBG_printf_E("memory allocation failed");
                    }
                    else if (reinterpret_cast<AtModeADCWebSocket *>(atModeADC)->init(interval, duration, packetSize, client)) {
                        args.print(F("Sending ADC readings to client=%p for %.2f seconds, interval=%.3f milliseconds, packet size=%u"), client, duration / 1000.0, interval / 1000.0, packetSize);
                    }
                    else {
                        args.print(F("Failed to initialize ADC"));
                        at_mode_adc_delete_object();
                    }
                }
                else {
                    args.print(F("Cannot find web socket client id=0x%08x"), clientId);
                }

            }
            else if (args.isAnyMatchIgnoreCase(0, F("0|off|stop"))) {
                args.print(F("ADC display off"));
                at_mode_adc_delete_object();
            }
            else {
                auto interval = args.toMillis(0, 100, ~0, 1000, String('s'));
                auto period = args.toIntMinMax(1, 1, 1000, 10);
                auto multiplier = args.toFloatMinMax(2, 0.1f, 100000.0f, 1000.0f);
                auto unit = args.toString(3, String('V'));
                auto readDelay = args.toIntMinMax(4, 0U, ~0U, 1250U);

                at_mode_adc_delete_object();
                atModeADC = new AtModeADC();
                if (!atModeADC) {
                    __DBG_printf_E("memory allocation failed");
                }
                else if (atModeADC->init(period, multiplier, unit, readDelay)) {

                    args.print(F("ADC display interval %ums"), interval);
                    auto &stream = args.getStream();
                    atModeADC->getTimer().add(interval, true, [&stream](Event::CallbackTimerPtr) {
                        stream.printf_P(PSTR("+ADC: %u (%umV) converted=%s "), atModeADC->getValue(), atModeADC->getValue(), atModeADC->getConvertedString().c_str());
                        atModeADC->printInfo(stream);
                    });
                }
                else {
                    args.print(F("Failed to initialize ADC"));
                    at_mode_adc_delete_object();
                }

            }
        }
    }
    #if ESP32
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CPU))) {
            if (perfmon_start(&const_cast<Stream &>(args.getStream())) == ESP_OK) {
                args.print(F("started"));
            }
            else if (perfmon_stop() == ESP_OK) {
                args.print(F("stopped"));
            }
            else {
                args.print(F("unknown error"));
            }
        }
    #elif defined(ESP8266) && (ARDUINO_ESP8266_MAJOR < 3)
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CPU))) {
            if (args.size() == 1) {
                auto speed = (uint8_t)args.toInt(0, ESP.getCpuFreqMHz());
                auto result = system_update_cpu_freq(speed);
                args.print(F("Set %d MHz = %d"), speed, result);
            }
            args.print(F("%d MHz"), ESP.getCpuFreqMHz());
        }
    #endif
#if DEBUG
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMP))) {

        auto version = System::Device::getConfig().config_version;
        auto versionStr = SaveCrash::Data::FirmwareVersion(version).toString();
        args.print(F("stored config version 0x%08x, %s"), version, versionStr.c_str());

        if (args.equalsIgnoreCase(0, F("json"))) {
            config.exportAsJson(output, versionStr);
        }
        else if (args.equalsIgnoreCase(0, F("dirty"))) {
            config.dump(output, true);
        }
        else if (args.size() == 1) {
            config.dump(output, false, args.toString(0));
        }
        else {
            config.dump(output);
        }
    }
    #if DEBUG
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPT))) {
            dumpTimers(output);
        }
    #endif
    #if DEBUG_CONFIGURATION_GETHANDLE
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPH))) {
            if (args.toLowerChar(0) == 'c') {
                ConfigurationHelper::writeHandles(true);
            }
            else if (args.toLowerChar(0) == 'p') {
                ConfigurationHelper::setPanicMode(true);
            }
            else {
                ConfigurationHelper::dumpHandles(output, args.toLowerChar(0) == 'l');
            }
        }
    #endif
    #if ESP8266
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPM))) {
            if (args.requireArgs(1, 4)) {
                uintptr_t start = translateAddress(args.toString(0));
                if (start == ~0U) {
                    start = args.toNumber(0, 0U);
                }
                auto len = args.toNumber(1, 32U);
                bool insecure = args.isTrue(2, false);
                bool flashRead = args.isTrue(3, true);
                if (!insecure && ((start < UMM_MALLOC_CFG_HEAP_ADDR || start >= 0x3FFFFFFFUL) && (start < SECTION_FLASH_START_ADDRESS || start >= SECTION_IROM0_TEXT_END_ADDRESS))) {
                    args.print(F("address=0x%08x not HEAP (%08x-%08x) or FLASH (%08x-%08x), use unsecure mode"), start, UMM_MALLOC_CFG_HEAP_ADDR, 0x3FFFFFFFUL, SECTION_FLASH_START_ADDRESS, SECTION_IROM0_TEXT_END_ADDRESS - 1);
                }
                else if (start & 0x3) {
                    args.print(F("address=%0x08x not aligned"), start);
                }
                else if (start == 0) {
                    args.print(F("address missing"));
                }
                else if (len == 0) {
                    args.print(F("length missing"));
                }
                else {
                    auto end = start + len;
                    args.print(F("start=0x%08x end=0x%08x length=%u"), start, end, end - start);
                    uint8_t buf[32];
                    std::fill_n(buf, sizeof(buf), 0xff);
                    while(start < end) {
                        auto len = std::min<size_t>(sizeof(buf), end - start);
                        if (flashRead && start >= SECTION_FLASH_START_ADDRESS && start < SECTION_FLASH_END_ADDRESS) {
                            if (!ESP.flashRead(start - SECTION_FLASH_START_ADDRESS, buf, len)) {
                                break;
                            }
                        }
                        else {
                            memcpy_P(buf, (const void *)start, len);
                        }
                        DumpBinary(args.getStream(), DumpBinary::kGroupBytesDefault, len, start).dump(buf, len);
                        start += len;
                    }
                }
            }
        }
    #endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPFS))) {
        at_mode_dump_fs_info(output);
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTCM))) {
        if (args.requireArgs(1)) {
            auto &stream = args.getStream();
            auto memoryId = static_cast<RTCMemoryManager::RTCMemoryId>(args.toNumber<uint32_t>(1));
            if (args.equalsIgnoreCase(0, F("qc")) || args.equalsIgnoreCase(0, F("quickconnect"))) {
                #if ENABLE_DEEP_SLEEP
                    config.storeQuickConnect(WiFi.BSSID(), WiFi.channel());
                    config.storeStationConfig(WiFi.localIP(), WiFi.subnetMask(), WiFi.gatewayIP());
                    args.print(F("Quick connect stored"));
                #else
                    args.print(F("Quick connect not available"));
                #endif
            }
            else if (args.equalsIgnoreCase(0, F("list")) || args.equalsIgnoreCase(0, F("info"))) {
                auto rtc = RTCMemoryManager::readTime();
                args.print(F("RTC time=%u status=%s"), rtc.getTime(), rtc.getStatus());
                args.print(F("RTC memory ids:"));
                for(uint8_t i = static_cast<uint8_t>(RTCMemoryManager::RTCMemoryId::NONE) + 1; i < static_cast<uint8_t>(RTCMemoryManager::RTCMemoryId::MAX); i++) {
                    stream.printf_P(PSTR("0x%02x      %s\n"), i, PluginComponent::getMemoryIdName(i));
                }
            }
            else if (args.equalsIgnoreCase(0, F("remove")) || args.equalsIgnoreCase(0, F("del")) || args.equalsIgnoreCase(0, F("delete")) || args.equalsIgnoreCase(0, F("rem"))) {
                if (memoryId != RTCMemoryManager::RTCMemoryId::NONE) {
                    uint32_t tmp;
                    if (RTCMemoryManager::read(memoryId, &tmp, sizeof(tmp))) {
                        RTCMemoryManager::remove(memoryId);
                        args.print(F("Data for id 0x%02x removed"), memoryId);
                    }
                    else {
                        args.print(F("No data for id 0x%02x available"), memoryId);
                    }
                }
            }
            else if (args.equalsIgnoreCase(0, F("clr")) || args.equalsIgnoreCase(0, F("clear"))) {
                if (memoryId != RTCMemoryManager::RTCMemoryId::NONE) {
                    RTCMemoryManager::remove(memoryId);
                    args.print(F("Data for id 0x%02x removed"), memoryId);
                }
                else {
                    RTCMemoryManager::clear();
                    args.print(F("RTC Memory cleared"));
                }
            }
            else if (args.equalsIgnoreCase(0, F("set")) || args.equalsIgnoreCase(0, F("write"))) {
                uint32_t data[32];
                int count = 0;
                for (uint8_t i = 2; i < 32 + 2 && i < args.size(); i++) {
                    data[count++] = args.toNumber(i, ~0U);
                }
                auto lengthInBytes = sizeof(data[0]) * count;
                RTCMemoryManager::write(memoryId, &data, lengthInBytes);
                stream.printf_P(PSTR("id=0x%02x length=%u dwords=%u cmd="), memoryId, lengthInBytes, (lengthInBytes + 3) / 4);
                if (count == 0) {
                    stream.printf_P(PSTR("+RTCM=remove,0x%02x\n"), memoryId);
                }
                else {
                    stream.printf_P(PSTR("+RTCM=set,0x%02x,"), memoryId);
                    for (uint8_t i = 0; i < count; i++) {
                        stream.printf_P(PSTR("0x%08x%c"), data[i], i == count - 1 ? '\n' : ',');
                    }
                }
            }
            else if (args.equalsIgnoreCase(0, F("get")) || args.equalsIgnoreCase(0, F("read"))) {
                uint32_t data[32];
                auto lengthInBytes = RTCMemoryManager::read(memoryId, &data, sizeof(data));
                auto count = (lengthInBytes + 3) / 4;
                stream.printf_P(PSTR("id=0x%02x length=%u dwords=%u cmd="), memoryId, lengthInBytes, (lengthInBytes + 3) / 4);
                if (count == 0) {
                    args.print(F("remove,0x%02x"), memoryId);
                }
                else {
                    stream.printf_P(PSTR("+RTCM=set,0x%02x,"), memoryId);
                    for (uint8_t i = 0; i < count; i++) {
                        stream.printf_P(PSTR("0x%08x%c"), data[i], i == count - 1 ? '\n' : ',');
                    }
                }
            }
            else {
                if (!RTCMemoryManager::dump(args.getStream(), memoryId) && memoryId != RTCMemoryManager::RTCMemoryId::NONE) {
                    args.print(F("No data for id 0x%02x available"), memoryId);
                }
            }
        }
    }
    #if LOGGER
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOGDBG))) {
            if (args.requireArgs(1, 1)) {
                bool enable = args.isTrue(0);
                static File debugLog;
                if (enable) {
                    if (!debugLog) {
                        _logger.setExtraFileEnabled(Logger::Level::_DEBUG, true);
                        _logger.__rotate(Logger::Level::_DEBUG);
                        debugLog = _logger.__openLog(Logger::Level::_DEBUG, true);
                        if (debugLog) {
                            debugStreamWrapper.add(&debugLog);
                            args.print(F("enabled=%s"), debugLog.fullName());
                        }
                    }
                }
                else {
                    if (debugLog) {
                        debugStreamWrapper.remove(&debugLog);
                        debugLog.close();
                        _logger.__rotate(Logger::Level::_DEBUG);
                        _logger.setExtraFileEnabled(Logger::Level::_DEBUG, false);
                    }
                }
                if (!debugLog) {
                    args.print(FSPGM(disabled));
                }
            }
        }
    #endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PANIC))) {
        if (args.equalsIgnoreCase(0, F("wdt"))) {
            args.print(F("starting a loop to trigger the WDT"));
            for(;;) {}
        }
        else if (args.equalsIgnoreCase(0, F("hwdt"))) {
            #if ESP8266
                ESP.wdtDisable();
                args.print(F("starting a loop to trigger the hardware WDT"));
                for(;;) {}
            #else
                args.print(F("not supported"));
            #endif
        }
        else if (args.equalsIgnoreCase(0, F("alloc"))) {
            uint32_t address = 0;
            args.print(F("writing zeros to memory @ 0x%08x (after malloc fails)"), address);
            delay(1000);
            while(malloc(4096)) {
            }
            #ifndef _MSC_VER
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wnonnull"
            #endif
                    memset((void *)address, 0, 2147483647);
                    memset((void *)2147483647, 0, 2147483647);
                    memset((void *)0, 0, 2147483647);
                }
                else if (args.size()) {
                    auto address = args.toNumber<uint32_t>(0);
                    args.print(F("writing zeros to memory @ 0x%08x"), address);
                    delay(1000);
                    memset((void *)address, 0, 2147483647);
                    memset((void *)2147483647, 0, 2147483647);
                    memset((void *)0, 0, 2147483647);
            #ifndef _MSC_VER
            #pragma GCC diagnostic pop
            #endif
                }
                else {
                    args.print(F("calling panic()"));
                    panic();
                }
            }
        #endif
        #if DEBUG_COLLECT_STRING_ENABLE
            else if (args.isCommand(PSTR("JSONSTRDUMP"))) {
                __debug_json_string_dump(output);
            }
        #endif
    else {
        bool commandWasHandled = false;
        for(const auto plugin: PluginComponents::Register::getPlugins()) { // send command to plugins
            if (plugin->hasAtMode()) {
                if (true == (commandWasHandled = plugin->atModeHandler(args))) {
                    break;
                }
            }
        }
        if (!commandWasHandled) {
            at_mode_print_invalid_command(output);
        }
    }

    if (nextCommand) {
        auto cmd = String(nextCommand);
        cmd.trim();
        if (cmd.length()) {
            at_mode_serial_handle_event(cmd);
        }
    }
    return;
}

void at_mode_serial_input_handler(Stream &client)
{
    if (is_at_mode_enabled) {
        static bool lastWasCR = false;
        auto &line = serialHandler.inputBuffer;

        auto serial = StreamWrapper(serialHandler.getStreams(), serialHandler.getInput()); // local output only
        while(client.available()) {
            int ch = client.read();
            // __DBG_printf("read %u (%c) cr=%u", (unsigned)((uint8_t)ch), isprint(ch) ? ch : '-', lastWasCR);
            if (lastWasCR == true && ch == '\n') {
                lastWasCR = false;
                continue;
            }
            switch(ch) {
                case 128:
                case -1:
                case 0:
                    break;
                case 9:
                    if (!line.length()) {
                        line = F("AT+");
                        serial.print(line);
                    }
                    break;
                case 8:
                    if (line.length()) {
                        line.remove(line.length() - 1, 1);
                        serial.print(F("\b \b"));
                    }
                    break;
                case '\n':
                    lastWasCR = false;
                    serial.write('\n');
                    at_mode_serial_handle_event(line);
                    line.clear();
                    break;
                case '\r':
                    lastWasCR = true;
                    serial.println();
                    at_mode_serial_handle_event(line);
                    line.clear();
                    break;
                default:
                    if (ch > 128) {
                        serial.printf_P(PSTR("Serial input - invalid character %u\r\n"), ch);
                        break;
                    }
                    line += (char)ch;
                    serial.write(ch);
                    if (line.length() >= SERIAL_HANDLER_INPUT_BUFFER_MAX) {
                        serial.write('\n');
                        at_mode_serial_handle_event(line);
                        line.clear();
                    }
                    break;
            }
        }
    }
}
