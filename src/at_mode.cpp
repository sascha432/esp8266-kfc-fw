/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <Syslog.h>
#include <ReadADC.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <StreamString.h>
#include <Cat.h>
#include <vector>
#include <JsonTools.h>
#include <ListDir.h>
#include "at_mode.h"
#include "kfc_fw_config.h"
#include "fs_mapping.h"
#include "logger.h"
#include "misc.h"
#include "web_server.h"
#include "web_socket.h"
#include "async_web_response.h"
#include "serial_handler.h"
#include "blink_led_timer.h"
#include "NeoPixel_esp.h"
#include "plugins.h"
#include "WebUIAlerts.h"
#include "PinMonitor.h"
#include "../src/plugins/http2serial/http2serial.h"
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
#include "../src/plugins/dimmer_module/dimmer_base.h"
#endif
#if IOT_BLINDS_CTRL
#include "../src/plugins/blinds_ctrl/blinds_plugin.h"
#endif
#include "umm_malloc/umm_malloc_cfg.h"

#include "core_esp8266_waveform.h"

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;
using KFCConfigurationClasses::MainConfig;

#include <AsyncWebSocket.h>

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

// append progmem strings to output and replace any whitespace with a single space
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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(AT, "Print OK", "Show help");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HELP, "HELP", "[single][,word][,or entire phrase]", "Search help");
#if ENABLE_DEEP_SLEEP
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DSLP, "DSLP", "[<milliseconds>[,<mode>]]", "Enter deep sleep");
#endif
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
#if ESP8266
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LSR, "LSR", "[<directory>]", "List files and directories using FS.openDir()");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "[<reset|on|off|ap_on|ap_off>]", "Modify WiFi settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(REM, "REM", "Ignore comment");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LED, "LED", "<slow,fast,flicker,off,solid,sos>,[,color=0xff0000][,pin]", "Set LED mode");
#if RTC_SUPPORT
PROGMEM_AT_MODE_HELP_COMMAND_DEF(RTC, "RTC", "[<set>]", "Set RTC time", "Display RTC time");
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PLG, "PLG", "<list|start|stop|add-blacklist|add|remove>[,<name>]", "Plugin management");

#if DEBUG

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ALERT, "ALERT", "<message>[,<type|0-3>]", "Add WebUI alert");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DSH, "DSH", "Display serial handler");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FSM, "FSM", "Display FS mapping");
#if PIN_MONITOR
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PINM, "PINM", "[<1=start|0=stop>,<interval in ms>]", "List or monitor PINs");
#endif
#if LOAD_STATISTICS
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap and system load");
#else
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RSSI, "RSSI", "[interval in seconds|0=disable]", "Display WiFi RSSI");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(GPIO, "GPIO", "[interval in seconds|0=disable]", "Display GPIO states");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PWM, "PWM", "<pin>,<input|waveform|level=0-" __STRINGIFY(PWMRANGE) ">[,<frequency=100-40000Hz>[,<duration/ms>]]", "PWM output on PIN, min./max. level set it to LOW/HIGH");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ADC, "ADC", "<off|display interval=1s>[,<period=1s>,<multiplier=1.0>,<unit=mV>,<read delay=5000us>]", "Read the ADC and display values");
#if defined(ESP8266)
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CPU, "CPU", "<80|160>", "Set CPU speed", "Display CPU speed");
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PSTORE, "PSTORE", "[<clear|remove|add>[,<key>[,<value>]]]", "Display/modify persistant storage");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(METRICS, "METRICS", "Display system metrics");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMP, "DUMP", "[<dirty|config.name>]", "Display settings");
#if DEBUG && ESP8266
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPT, "DUMPT", "Dump timers");
#endif
#if DEBUG_CONFIGURATION_GETHANDLE
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPH, "DUMPH", "[<log|panic|clear>]", "Dump configuration handles");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPM, "DUMPM", "<start>,<length>", "Dump memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPA, "DUMPA", "<reset|mark|leak|freed>", "Memory allocation statistics");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPFS, "DUMPFS", "Display file system information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPEE, "DUMPEE", "[<offset>[,<length>]", "Dump EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WRTC, "WRTC", "<id,data>", "Write uint32 to RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPRTC, "DUMPRTC", "Dump RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RTCCLR, "RTCCLR", "Clear RTC memory");
#if ENABLE_DEEP_SLEEP
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RTCQCC, "RTCQCC", "<0=channel/bssid|1=static ip config.>", "Clear quick connect RTC memory");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIMO, "WIMO", "<0=off|1=STA|2=AP|3=STA+AP>", "Set WiFi mode, store configuration and reboot");
#if LOGGER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOGDBG, "LOGDBG", "<1|0>", "Enable/disable writing debug output to log://debug");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PANIC, "PANIC", "Cause an exception by calling panic()");

#endif

void at_mode_help_commands()
{
    auto name = PSTR("at_mode");
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(AT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HELP), name);
#if ENABLE_DEEP_SLEEP
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DSLP), name);
#endif
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
#if ESP8266
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LSR), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(WIFI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LED), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(REM), name);
#if RTC_SUPPORT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RTC), name);
#endif

#if DEBUG
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
#if defined(ESP8266)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(CPU), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PSTORE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(METRICS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMP), name);
#if DEBUG && ESP8266
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPT), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPA), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPFS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPEE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPRTC), name);
#if DEBUG_CONFIGURATION_GETHANDLE
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(DUMPH), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(WRTC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RTCCLR), name);
#if ENABLE_DEEP_SLEEP
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RTCQCC), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(WIMO), name);
#if LOGGER
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(LOGDBG), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PANIC), name);
#endif

}

static void new_ATModeHelpVector_atModeCommandHelp()
{
#if DEBUG
    if (atModeCommandHelp) {
        __DBG_panic("atModeCommandHelp=%p", atModeCommandHelp);
    }
#endif
    atModeCommandHelp = __LDBG_new(ATModeHelpVector);
}

void at_mode_generate_help(Stream &output, StringVector *findText = nullptr)
{
    __LDBG_printf("find=%s", findText ? implode(',', *findText).c_str() : PSTR("nullptr"));

    new_ATModeHelpVector_atModeCommandHelp();

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin: plugins) {
        plugin->atModeHelpGenerator();
    }
    at_mode_display_help(output, findText);

    __LDBG_delete(atModeCommandHelp);
    atModeCommandHelp = nullptr;

    if (config.isSafeMode()) {
        output.printf_P(PSTR("\nSAFE MODE ENABLED\n\n"));
    }
}

String at_mode_print_command_string(Stream &output, char separator)
{
    String commands;
    StreamString nullStream;

    new_ATModeHelpVector_atModeCommandHelp();

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin : plugins) {
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

    __LDBG_delete(atModeCommandHelp);
    atModeCommandHelp = nullptr;

    return commands;
}

#if DEBUG

class DisplayTimer {
public:
    typedef enum {
        HEAP = 1,
        RSSI = 2,
        GPIO = 3,
    } DisplayTypeEnum_t;

    DisplayTimer() : _type(HEAP) {
    }

    bool removeTimer() {
        return _timer.remove();
    }

public:
    Event::Timer _timer;
    DisplayTypeEnum_t _type = HEAP;
    int32_t _rssiMin, _rssiMax;
};

DisplayTimer displayTimer;

static void print_heap()
{
    Serial.printf_P(PSTR("+HEAP: free=%u cpu=%dMHz frag=%u"), ESP.getFreeHeap(), ESP.getCpuFreqMHz(), ESP.getHeapFragmentation());
#if HAVE_MEM_DEBUG
    KFCMemoryDebugging::dumpShort(Serial);
#endif
}
static void heap_timer_callback(Event::CallbackTimerPtr timer)
{
    if (displayTimer._type == DisplayTimer::HEAP) {
        print_heap();
#if LOAD_STATISTICS
        Serial.printf_P(PSTR(" load avg=%.2f %.2f %.2f"), LOOP_COUNTER_LOAD(load_avg[0]), LOOP_COUNTER_LOAD(load_avg[1]), LOOP_COUNTER_LOAD(load_avg[2]));
#endif
        Serial.println();
    }
    else if (displayTimer._type == DisplayTimer::GPIO) {
        Serial.printf_P(PSTR("+GPIO: "));
#if defined(ESP8266)
        for(uint8_t i = 0; i < NUM_DIGITAL_PINS; i++) {
            if (i != 1 && i != 3 && !isFlashInterfacePin(i)) { // do not display RX/TX and flash SPI
                // pinMode(i, INPUT);
                Serial.printf_P(PSTR("%u=%u "), i, digitalRead(i));
            }
        }
        // pinMode(A0, INPUT);
        Serial.printf_P(PSTR(" A0=%u\n"), analogRead(A0));
#else
// #warning not implemented
#endif
    }
    else {
        int32_t rssi = WiFi.RSSI();
        displayTimer._rssiMin = std::max(displayTimer._rssiMin, rssi);
        displayTimer._rssiMax = std::min(displayTimer._rssiMax, rssi);
        Serial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, displayTimer._rssiMin, displayTimer._rssiMax);
    }
}

static void create_heap_timer(float seconds, DisplayTimer::DisplayTypeEnum_t type = DisplayTimer::HEAP)
{
    displayTimer._type = type;
    _Timer(displayTimer._timer).add(Event::seconds(seconds), true, heap_timer_callback);
}

void at_mode_create_heap_timer(float seconds)
{
    if (seconds) {
        create_heap_timer(seconds, DisplayTimer::HEAP);
    } else {
        displayTimer.removeTimer();
    }
}

#endif

void at_mode_wifi_callback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        Serial.printf_P(PSTR("WiFi connected to %s - IP %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        Serial.println(F("WiFi connection lost"));
    }
}

SerialHandler::Client *_client;

void at_mode_setup()
{
    __DBG_printf("AT_MODE_ENABLED=%u", System::Flags::getConfig().is_at_mode_enabled);

    _client = &serialHandler.addClient(at_mode_serial_input_handler, SerialHandler::EventType::READ);

    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, at_mode_wifi_callback);
}

void enable_at_mode(Stream &output)
{
    if (!System::Flags::getConfig().is_at_mode_enabled) {
        output.println(F("Enabling AT MODE."));
        System::Flags::getWriteableConfig().is_at_mode_enabled = true;
        _client->start(SerialHandler::EventType::READ);
    }
}

void disable_at_mode(Stream &output)
{
    if (System::Flags::getConfig().is_at_mode_enabled) {
        _client->stop();
        output.println(F("Disabling AT MODE."));
#if DEBUG
        displayTimer.removeTimer();
#endif
        System::Flags::getWriteableConfig().is_at_mode_enabled = false;
    }
}

void at_mode_dump_fs_info(Stream &output)
{
    FSInfo info;
    SPIFFS_info(info);
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
    output.println("AT? or AT+HELP=<command|text to find> for help");
}

void at_mode_print_invalid_command(Stream &output)
{
    output.print(F("ERROR - Invalid command. "));
    if (config.isSafeMode()) {
        output.print(F(""));
    }
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

#if DEBUG && ESP8266

extern ETSTimer *timer_list;

void at_mode_list_ets_timers(Print &output)
{
    ETSTimer *cur = timer_list;
    while(cur) {
        void *callback = nullptr;
        for(const auto &timer: __Scheduler.__getTimers()) {
            if (&timer->_etsTimer == cur) {
                callback = lambda_target(timer->_callback);
                break;
            }
        }
        float period_in_s = cur->timer_period / 312500.0;
        output.printf_P(PSTR("ETSTimer=%p func=%p arg=%p period=%u (%.3fs) exp=%u callback=%p\n"),
            cur,
            cur->timer_func,
            cur->timer_arg,
            cur->timer_period,
            period_in_s,
            cur->timer_expire,
            callback);

        cur = cur->timer_next;
    }
#if DEBUG_EVENT_SCHEDULER
    output.println(F("Event::Scheduler"));
    __Scheduler.__list(false);
#endif
}

#endif

static void at_mode_adc_loop();
static void at_mode_adc_delete_object();

class AtModeADC {
public:
    AtModeADC() : _adcIntegralMultiplier(0), _adc(ADCManager::getInstance()) {
    }
    virtual ~AtModeADC() {
        _displayTimer.remove();
        if (_adcIntegralMultiplier) {
            LoopFunctions::remove(at_mode_adc_loop);
        }
    }

    // useable values for integrating the average value is 1-100 (averagePeriodMillis)
    bool init(uint16_t averagePeriodMillis, float convertMultiplier, const String &unit, uint32_t readDelayMicros = 5000) {
        _displayTimer.remove();
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
        LoopFunctions::add(at_mode_adc_loop);
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
        _displayTimer.remove();
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
        LoopFunctions::add(at_mode_adc_loop);
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

    void resetBuffer() {
        _buffer.setLength(0);
        _buffer.reserve(_webSocket.packetSize + 1);
        Header_t header = { WsClient::BinaryPacketType::ADC_READINGS, FLAGS_NONE, sizeof(Data_t) };
        _buffer.push_back(header);
    }

    virtual void processData(uint16_t reading, uint32_t diff, uint32_t micros) override {

        Data_t data;
        uint32_t ms = micros / 1000U;
        auto time = get_time_diff(_webSocket.start, ms);

        if (_buffer.length() + sizeof(data) >= _webSocket.packetSize) {

            bool endReading = (time > _webSocket.duration);

            if (Http2Serial::getClientById(_client)) {
                if (_client->canSend()) {
                    uint8_t *ptr;
                    _buffer.write(0); // terminate with NUL byte
                    size_t len = _buffer.length() - 1;
                    _buffer.move(&ptr);
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

                    auto wsBuffer = _client->server()->makeBuffer(ptr, len, false);
                    if (wsBuffer) {
                        _client->binary(wsBuffer);
                    }
                }
                else {
                    //__DBG_printf("dropped ADC packet size=%u", _buffer.length());
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

AtModeADC *atModeADC = nullptr;

static void at_mode_adc_delete_object()
{
    if (atModeADC) {
        __LDBG_delete(atModeADC);
        atModeADC = nullptr;
    }
}

static void at_mode_adc_loop() {
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

void at_mode_serial_handle_event(String &commandString)
{
    auto &output = Serial;
    char *nextCommand = nullptr;
    AtModeArgs args(output);

    commandString.trim();
    bool isQueryMode = commandString.length() && (commandString.charAt(commandString.length() - 1) == '?');

    if (!strncasecmp_P(commandString.c_str(), PSTR("AT"), 2)) {
        commandString.remove(0, 2);
#if AT_MODE_ALLOW_PLUS_WITHOUT_AT
    // allow using AT+COMMAND and +COMMAND
    } else if (commandString.charAt(0) == '+') {
#endif
    } else {
        at_mode_print_invalid_command(output);
        return;
    }

    if (commandString.length() == 0) { // AT
        args.ok();
    }
    else {
        if (commandString.length() == 1 && commandString.charAt(0) == '?') { // AT?
            at_mode_generate_help(output);
        }
        else {
#if defined(ESP8266)
            auto command = commandString.begin();
#else
            std::unique_ptr<char []> __command(new char[commandString.length() + 1]);
            auto command = strcpy(__command.get(), commandString.c_str());
#endif
            command++;

            char *ptr = strchr(command, '?');
            if (isQueryMode && ptr) { // query mode has no arguments
                *ptr = 0;
                args.setQueryMode(true);
            } else {
                __LDBG_printf("tokenizer('%s')", command);
                args.setQueryMode(false);
                tokenizer(command, args, true, &nextCommand);
            }
            __LDBG_printf("cmd=%s,argc=%d,args='%s',next_cmd='%s'", command, args.size(), implode(F("','"), args.getArgs()).c_str(), nextCommand ? nextCommand : String(0).c_str());

            args.setCommand(command);

#if ENABLE_DEEP_SLEEP
            if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DSLP))) {
                KFCFWConfiguration::milliseconds time(args.toMillis(0));
                RFMode mode = (RFMode)args.toInt(1, RF_DEFAULT);
                args.print(F("Entering deep sleep..."));
                config.enterDeepSleep(time, mode, 1);
            }
            else
#endif
            if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HELP))) {
                String plugin;
                StringVector findItems;
                for(auto strPtr: args.getArgs()) {
                    String str = strPtr;
                    str.trim();
                    str.toLowerCase();
                    findItems.push_back(str);
                }
                at_mode_generate_help(output, &findItems);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(METRICS))) {

                args.printf_P(PSTR("Device name: %s"), System::Device::getName());
                args.printf_P(PSTR("Uptime: %u seconds / %s"), getSystemUptime(), formatTime(getSystemUptime(), true).c_str());
                args.printf_P(PSTR("Free heap/fragmentation: %u / %u"), ESP.getFreeHeap(), ESP.getHeapFragmentation());
                args.printf_P(PSTR("Heap start/size: 0x%x/%u"), SECTION_HEAP_START_ADDRESS, SECTION_HEAP_END_ADDRESS - SECTION_HEAP_START_ADDRESS);
                args.printf_P(PSTR("irom0.text: 0x%08x-0x%08x"), SECTION_IROM0_TEXT_START_ADDRESS, SECTION_IROM0_TEXT_END_ADDRESS);
                args.printf_P(PSTR("CPU frequency: %uMHz"), ESP.getCpuFreqMHz());
                args.printf_P(PSTR("Flash size: %s"), formatBytes(ESP.getFlashChipRealSize()).c_str());
                args.printf_P(PSTR("Firmware size: %s"), formatBytes(ESP.getSketchSize()).c_str());
                args.printf_P(PSTR("EEPROM: 0x%x"), SECTION_EEPROM_START_ADDRESS);
                args.printf_P(PSTR("EspSaveCrash: 0x%x"), SECTION_ESPSAVECRASH_START_ADDRESS);
                args.printf_P(PSTR("KFCFW: 0x%x/%u"), SECTION_KFCFW_START_ADDRESS, SECTION_KFCFW_END_ADDRESS - SECTION_KFCFW_START_ADDRESS);
                args.printf_P(PSTR("WiFiCallbacks: size=%u count=%u"), sizeof(WiFiCallbacks::Entry), WiFiCallbacks::getVector().size());
                args.printf_P(PSTR("LoopFunctions: size=%u count=%u"), sizeof(LoopFunctions::Entry), LoopFunctions::getVector().size());

                args.printf_P(PSTR("sizeof(String) / SSOSIZE: %u / %u"), sizeof(String), StringSSOSize::getSSOSize());
                args.printf_P(PSTR("sizeof(std::vector<int>): %u"), sizeof(std::vector<int>));
                args.printf_P(PSTR("sizeof(std::vector<double>): %u"), sizeof(std::vector<double>));
                args.printf_P(PSTR("sizeof(std::list<int>): %u"), sizeof(std::list<int>));
                // args.printf_P(PSTR("sizeof(FormUI::Field::Base): %u"), sizeof(FormUI::Field::Base));
                // args.printf_P(PSTR("sizeof(FormUI:UI): %u"), sizeof(FormUI::WebUI::Base));
                args.printf_P(PSTR("sizeof(AsyncClient): %u"), sizeof(AsyncClient));
                args.printf_P(PSTR("sizeof(CallbackTimer): %u"), sizeof(Event::CallbackTimer));
                // args.printf_P(PSTR("sizeof(SerialTwoWire): %u"), sizeof(SerialTwoWire));

#if DEBUG
                String hash;
                if (System::Firmware::getElfHashHex(hash)) {
                    args.printf_P(PSTR("Firmware ELF hash: %s"), hash.c_str());
                }
#endif
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RST))) {
                bool safeMode = false;
                if (args.equals(0, 's')) {
                    safeMode = true;
                    args.print(F("Software reset, safe mode enabled..."));
                }
                else {
                    args.print(F("Software reset..."));
                }
                config.restartDevice(safeMode);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CMDS))) {
                output.print(F("+CMDS="));
                at_mode_print_command_string(output, ',');
                output.println();
            }
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
                            Configuration::Handle_t handles[AT_MODE_MAX_ARGUMENTS + 1];
                            auto iterator = args.begin();
                            if (++iterator != args.end()) {
                                output.print(F(": "));
                                handlesPtr = &handles[0];
                                auto count = 0;
                                while(iterator != args.end() && count < AT_MODE_MAX_ARGUMENTS) {
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
                            args.printf_P(PSTR("Failed to import: %s"), filename);
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
                config.restartDevice(false);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ATMODE))) {
                if (args.requireArgs(1, 1)) {
                    if (args.isTrue(0)) {
                        enable_at_mode(output);
                    } else {
                        disable_at_mode(output);
                    }
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LED))) {
                if (args.requireArgs(1, 3)) {
                    String mode = args.toString(0);
                    int32_t color = args.toNumber(1, -1);
                    int8_t pin = (int8_t)args.toInt(2, __LED_BUILTIN);
                    mode.toUpperCase();
                    if (args.equalsIgnoreCase(0, F("slow"))) {
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::SLOW, color);
                    }
                    else if (args.equalsIgnoreCase(0, F("fast"))) {
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::FAST, color);
                    }
                    else if (args.equalsIgnoreCase(0, F("flicker"))) {
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::FLICKER, color);
                    }
                    else if (args.equalsIgnoreCase(0, F("solid"))) {
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::SOLID, color);
                    }
                    else if (args.equalsIgnoreCase(0, F("sos"))) {
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::SOS, color);
                    }
                    else {
                        mode = F("OFF");
                        BlinkLEDTimer::setBlink(pin, BlinkLEDTimer::OFF);
                    }
                    args.printf_P(PSTR("LED pin=%d, mode=%s, color=0x%06x"), pin, mode.c_str(), color);
                }
            }
            else if (args.isCommand(F("I2CT")) || args.isCommand(F("I2CA")) || args.isCommand(F("I2CR"))) {
                // ignore SerialTwoWire communication
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WIFI))) {
                if (!args.empty()) {
                    auto arg0 = args.toString(0);
                    if (String_equalsIgnoreCase(arg0, FSPGM(on))) {
                        args.print(F("enabling station mode"));
                        WiFi.enableSTA(true);
                        WiFi.reconnect();
                    }
                    else if (String_equalsIgnoreCase(arg0, FSPGM(off))) {
                        args.print(F("disabling station mode"));
                        WiFi.enableSTA(false);
                    }
                    else if (String_equalsIgnoreCase(arg0, F("ap_on"))) {
                        args.print(F("enabling AP mode"));
                        WiFi.enableAP(true);
                    }
                    else if (String_equalsIgnoreCase(arg0, F("ap_off"))) {
                        args.print(F("disabling AP mode"));
                        WiFi.enableAP(false);
                    }
                    else if (String_equalsIgnoreCase(arg0, F("reset"))) {
                        config.reconfigureWiFi();
                    }
                }

                auto flags = System::Flags::getConfig();
                args.printf_P("station mode %s, DHCP %s, SSID %s, connected %s, IP %s",
                    (WiFi.getMode() & WIFI_STA) ? SPGM(on) : SPGM(off),
                    flags.is_station_mode_dhcp_enabled ? SPGM(on) : SPGM(off),
                    Network::WiFi::getSSID(),
                    WiFi.isConnected() ? SPGM(yes) : SPGM(no),
                    WiFi.localIP().toString().c_str()
                );
                args.printf_P("AP mode %s, DHCP %s, SSID %s, clients connected %u, IP %s",
                    (flags.is_softap_enabled) ? ((flags.is_softap_standby_mode_enabled) ? ((WiFi.getMode() & WIFI_AP) ? SPGM(on) : PSTR("stand-by")) : SPGM(on)) : SPGM(off),
                    flags.is_softap_dhcpd_enabled ? SPGM(on) : SPGM(off),
                    Network::WiFi::getSoftApSSID(),
                    WiFi.softAPgetStationNum(),
                    Network::SoftAP::getConfig().getAddress().toString().c_str()
                );
            }
#if RTC_SUPPORT
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTC))) {
                if (!args.empty()) {
                    args.printf_P(PSTR("Time=%u, rtc=%u, lostPower=%u"), (uint32_t)time(nullptr), config.getRTC(), config.rtcLostPower());
                    output.print(F("+RTC: "));
                    config.printRTCStatus(output);
                    output.println();
                }
                else {
                    args.printf_P(PSTR("Set=%u, rtc=%u"), config.setRTC(time(nullptr)), config.getRTC());
                }
            }
#endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(REM))) {
                // ignore comment
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DLY))) {
                auto delayTime = args.toMillis(0, 1, 3000, 1);
                args.printf_P(PSTR("%lu"), delayTime);
                delay(delayTime);
                args.ok();
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RM))) {
                if (args.requireArgs(1, 1)) {
                    auto filename = args.get(0);
                    auto result = KFCFS.remove(filename);
                    args.printf_P(PSTR("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RN))) {
                if (args.requireArgs(2, 2)) {
                    auto filename = args.get(0);
                    auto newFilename = args.get(0);
                    auto result = KFCFS.rename(filename, newFilename);
                    args.printf_P(PSTR("%s => %s: %s"), filename, newFilename, result ? PSTR("success") : PSTR("failure"));
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
#if ESP8266
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LSR))) {
                auto dir = KFCFS.openDir(args.toString(0));
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
#endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CAT))) {
                if (args.requireArgs(1, 1)) {
                    StreamOutput::Cat::dump(args.toString(0), output, StreamOutput::Cat::kPrintInfo|StreamOutput::Cat::kPrintCrLfAsText);
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PLG))) {

                if (args.requireArgs(1, 2)) {
                    auto cmds = PSTR("list|start|stop|add-blacklist|add|remove");
                    int cmd = stringlist_find_P_P(cmds, args.get(0), '|');
                    if (cmd == 0) {
                        dump_plugin_list(output);
                        args.printf_P("Blacklist=%s", PluginComponent::getBlacklist());
                    }
                    else if (args.requireArgs(2, 2)) {
                        PluginComponent *plugin = nullptr;
                        plugin = PluginComponent::findPlugin(FPSTR(args.get(1)), false);
                        if (!plugin) {
                            args.printf_P(PSTR("Cannot find plugin '%s'"), args.get(1));
                        }
                        else {
                            switch(cmd) {
                                case 1: // start
                                    if (plugin->getSetupTime() == 0) {
                                        args.printf_P(PSTR("Calling %s.setup()"), plugin->getName_P());
                                        plugin->setSetupTime();
                                        plugin->setup(PluginComponent::SetupModeType::DEFAULT);
                                    }
                                    else {
                                        args.printf_P(PSTR("%s already running"), plugin->getName_P());
                                    }
                                    break;
                                case 2: // stop
                                    if (plugin->getSetupTime() != 0) {
                                        args.printf_P(PSTR("Calling %s.shutdown()"), plugin->getName_P());
                                        plugin->shutdown();
                                        plugin->clearSetupTime();
                                    }
                                    else {
                                        args.printf_P(PSTR("%s not running"), plugin->getName_P());
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
                                        args.printf_P("Blacklist=%s action=%s", PluginComponent::getBlacklist(), flag ? SPGM(success) : SPGM(failure));
                                    } break;
                                default:
                                    args.printf_P(PSTR("expected <%s>"), cmds);
                                    break;
                            }
                        }
                    }
                }
            }
#if DEBUG
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
                auto &streams = *serialHandler.getStreams();
                auto &clients = serialHandler.getClients();
                args.printf_P(PSTR("outputs=%u clients=%u"), streams.size(), clients.size());
                for(const auto stream: streams) {
                    args.printf_P(PSTR("output %p"), stream);
                }
                args.printf_P(PSTR("input %p"), serialHandler.getInput());
                for(auto &client: clients) {
                    args.printf_P(PSTR("client %p: events: %02x"), &client, client.getEvents());
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
                // Mappings::getInstance().dump(output);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ALERT))) {
                if (args.size() > 0) {
                    auto msg = args.toString(0);
                    auto id = WebAlerts::Alert::add(args.toString(0), static_cast<WebAlerts::Type>(args.toIntMinMax(1, (int)(WebAlerts::Type::NONE) + 1, (int)(WebAlerts::Type::MAX) - 1, (int)(WebAlerts::Type::SUCCESS))));
                    args.printf_P(PSTR("Alert added id %u"), id);
                }
                if (WebAlerts::Alert::hasOption(WebAlerts::OptionsType::GET_ALERTS)) {
                    args.print(F("--- Alerts ---"));
                    // for(auto &alert: WebAlerts::Alert::getAlerts()) {
                    //     String str;
                    //     WebAlerts::Message::toString(str, alert);
                    //     String_rtrim_P(str, PSTR("\r\n"));
                    //     args.print(str.c_str());
                    // }
                }
            }
#if PIN_MONITOR
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PINM))) {
                if (args.isTrue(0) || !pinMonitor.isDebugRunning()) {
                    args.print(F("starting debug mode"));
                    pinMonitor.beginDebug(args.getStream(), args.toMillis(1, 500, ~0, 1000U));
                }
                else {
                    args.print(F("ending debug mode"));
                    pinMonitor.endDebug();
                }
            }
#endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                if (args.requireArgs(0, 1)) {
                    auto interval = args.toMillis(0, 500, ~0, 0, String('s'));
                    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI))) {
                        displayTimer._type = DisplayTimer::RSSI;
                    }
                    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                        displayTimer._type = DisplayTimer::GPIO;
                    }
                    else {
                        displayTimer._type = DisplayTimer::HEAP;
                    }
                    displayTimer._rssiMin = -10000;
                    displayTimer._rssiMax = 0;
                    if (interval == 0) {
                        if (displayTimer.removeTimer()) {
                            args.print(F("Interval disabled"));
                        }
                        else {
                            print_heap();
                            Serial.println();
                        }
                    } else {
                        float fInterval = interval / 1000.0;
                        args.printf_P(PSTR("Interval set to %.3f seconds"), fInterval);
                        create_heap_timer(fInterval, displayTimer._type);
                    }
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PWM))) {
                if (args.requireArgs(2, 7)) {
                    auto pin = (uint8_t)args.toInt(0);
                    if (args.equalsIgnoreCase(1, F("waveform"))) {
                        uint32_t timeHighUS = args.toInt(2, ~0U);
                        uint32_t timeLowUS = args.toInt(3, ~0U);
                        uint32_t runTimeUS = args.toInt(4, 0);
                        uint32_t increment = args.toInt(5, 0);
                        uint32_t delayTime = args.toInt(6);
                        if (timeHighUS == ~0U || timeLowUS == ~0U) {
                            digitalWrite(pin, LOW);
                            stopWaveform(pin);
                            pinMode(pin, INPUT);
                            args.print(F("usage: <pin>,waveform,<high-time cycles>,<low-time cycles>[,<run-time cycles|0=unlimited>,<increment>,<delay ms>]"));
                        }
                        else {
                            digitalWrite(pin, LOW);
                            pinMode(pin, OUTPUT);
                            if (increment) {
                                args.printf_P(PSTR("pin=%u high=%u low=%u runtime=%u increment=%u delay=%u"), pin, timeHighUS, timeLowUS, runTimeUS, increment, delayTime);
                                uint32_t start = 0;
                                _Scheduler.add(Event::milliseconds(delayTime), true, [args, delayTime, pin, timeHighUS, timeLowUS, runTimeUS, increment, start](Event::CallbackTimerPtr timer) mutable {
                                    start += increment;
                                    if (start >= timeHighUS) {
                                        start = timeHighUS;
                                        timer->disarm();
                                    }
                                    startWaveformClockCycles(pin, start, timeLowUS, runTimeUS);
                                    if (delayTime > 20) {
                                        args.printf_P(PSTR("pin=%u high=%u low=%u runtime=%u"), pin, start, timeLowUS, runTimeUS);
                                    }
                                }, Event::PriorityType::TIMER);

                            } else {
                                args.printf_P(PSTR("pin=%u high=%u low=%u runtime=%u"), pin, timeHighUS, timeLowUS, runTimeUS);
                                startWaveformClockCycles(pin, timeHighUS, timeLowUS, runTimeUS);
                            }

                        }
                    }
                    else if (args.equalsIgnoreCase(1, F("input"))) {

                        digitalWrite(pin, LOW);
                        pinMode(pin, INPUT);
                        args.printf_P(PSTR("set pin=%u to INPUT"), pin);
                    }
                    else {

                        // +PWM=4,512,40000,2000

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
                        pinMode(pin, OUTPUT);
#if defined(ESP8266)
                        analogWriteFreq(freq);
                        analogWriteRange(PWMRANGE);
#else
                        freq = 0;
#endif
                        auto type = PSTR("digitalWrite");
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
                            args.printf_P(PSTR("%s(%u, %u)%s"), type, pin, level, durationStr.c_str());
                        }
                        else {
                            float cycle = (1000000 / (float)freq);
                            float dc = cycle * (level / (float)PWMRANGE);
                            args.printf_P(PSTR("%s(%u, %u) duty cycle=%.2f cycle=%.2fµs f=%uHz%s"), type, pin, level, dc, cycle, freq, durationStr.c_str());
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
                            atModeADC = __LDBG_new(AtModeADCWebSocket);
                            if (reinterpret_cast<AtModeADCWebSocket *>(atModeADC)->init(interval, duration, packetSize, client)) {
                                args.printf_P(PSTR("Sending ADC readings to client=%p for %.2f seconds, interval=%.3f milliseconds, packet size=%u"), client, duration / 1000.0, interval / 1000.0, packetSize);
                            }
                            else {
                                args.print(F("Failed to initialize ADC"));
                                at_mode_adc_delete_object();
                            }
                        }
                        else {
                            args.printf_P(PSTR("Cannot find web socket client id=0x%08x"), clientId);
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
                        atModeADC = __LDBG_new(AtModeADC);
                        if (atModeADC->init(period, multiplier, unit, readDelay)) {

                            args.printf_P(PSTR("ADC display interval %ums"), interval);
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
    #if defined(ESP8266)
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CPU))) {
                if (args.size() == 1) {
                    auto speed = (uint8_t)args.toInt(0, ESP.getCpuFreqMHz());
                    auto result = system_update_cpu_freq(speed);
                    args.printf_P(PSTR("Set %d MHz = %d"), speed, result);
                }
                args.printf_P(PSTR("%d MHz"), ESP.getCpuFreqMHz());
            }
    #endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMP))) {
                if (args.equals(0, F("dirty"))) {
                    config.dump(output, true);
                }
                else if (args.size() == 1) {
                    config.dump(output, false, args.toString(0));
                }
                else {
                    config.dump(output);
                }
            }
#if DEBUG && ESP8266
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPT))) {
                at_mode_list_ets_timers(output);
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPM))) {
                if (args.requireArgs(2, 3)) {
                    uint32_t start = args.toNumber(0);
                    uint32_t len = args.toNumber(1);
                    if (!start || !len) {
                        args.print(F("start or length missing"));
                    }
                    else {
                        uint8_t *startPtr = (uint8_t *)(start & ~0x3);
                        uint8_t *endPtr = (uint8_t *)((start + len + 3) & ~0x03);

                        args.printf_P(PSTR("start=0x%08x end=0x%08x"), startPtr, endPtr);
                        if (args.isTrue(2)) {

                            auto &stream = args.getStream();

                            constexpr size_t kPerColumn = 4;
                            uint32_t data[kPerColumn];
                            uint32_t *dword = &data[0];
                            size_t col = 0;
                            stream.printf_P("\n%08x: ", (uintptr_t)startPtr);
                            while(startPtr < endPtr) {
                                if ((uintptr_t)startPtr < SECTION_FLASH_START_ADDRESS) {
                                    *dword = pgm_read_dword(startPtr);
                                }
                                else if (!ESP.flashRead((uintptr_t)startPtr - SECTION_FLASH_START_ADDRESS, dword, sizeof(*dword))) {
                                    *dword = ~0;
                                }
                                startPtr += sizeof(*dword);
                                stream.printf_P(PSTR("%08x "), *dword);
                                dword++;

                                if (++col % kPerColumn == 0) {
                                    dword = &data[0];
                                    stream.print('[');
                                    printable_string(stream, (const uint8_t *)data, sizeof(data));
                                    if (startPtr < endPtr) {
                                        stream.printf_P("]\n%08x: ", (uintptr_t)startPtr);
                                    }
                                }
                            }
                            stream.println();
                        }

                    }
                }
            }
#if HAVE_MEM_DEBUG
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPA))) {
                KFCMemoryDebugging::AllocType type = KFCMemoryDebugging::AllocType::ALL;
                int cmd = -1;
                if (args.size() >= 1) {
                    cmd = stringlist_find_P_P(PSTR("reset|mark|leak|freed"), args.get(0), '|');
                    switch(cmd) {
                        case 0: // reset
                            args.print(F("stats reset"));
                            KFCMemoryDebugging::reset();
                            cmd = 0;
                            break;
                        case 1: // mark
                            args.print(F("all blocks marked as no leak"));
                            KFCMemoryDebugging::markAllNoLeak();
                            cmd = 0;
                            break;
                        case 2: // leak
                            type = KFCMemoryDebugging::AllocType::LEAK;
                            break;
                        case 3: // freed
                            type = KFCMemoryDebugging::AllocType::FREED;
                            break;
                        default:
                            break;
                    }
                }
                if (cmd) {
                    KFCMemoryDebugging::dump(Serial, type);
                }
            }
#endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPFS))) {
                at_mode_dump_fs_info(output);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPEE))) {
                uint16_t offset = args.toInt(0);
                uint16_t length = args.toInt(1, 1);
                config.dumpEEPROM(output, false, offset, length);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WRTC))) {
                if (args.requireArgs(2, 2)) {
                    auto rtcMemId = static_cast<RTCMemoryManager::RTCMemoryId>(args.toInt(0));
                    uint32_t data = (uint32_t)args.toNumber(1);
                    RTCMemoryManager::write(rtcMemId, &data, sizeof(data));
                    args.printf_P(PSTR("id=%u, data=%u (%x)"), rtcMemId, data, data);
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPRTC))) {
                RTCMemoryManager::dump(Serial);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTCCLR))) {
                RTCMemoryManager::clear();
                args.print(F("Memory cleared"));
            }
#if ENABLE_DEEP_SLEEP
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTCQCC))) {
                if (args.requireArgs(1, 1)) {
                    int num = args.toInt(0);
                    if (num == 0) {
                        uint8_t bssid[6];
                        memset(bssid, 0, sizeof(bssid));
                        config.storeQuickConnect(bssid, -1);
                        args.print(F("BSSID and channel removed"));
                    } else if (num == 1) {
                        config.storeStationConfig(0, 0, 0);
                        args.print(F("Static IP info removed"));
                    } else {
                        at_mode_print_invalid_arguments(output);
                    }
                }
            }
#endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WIMO))) {
                if (args.requireArgs(1, 1)) {
                    args.print(F("Setting WiFi mode and restarting device..."));
                    System::Flags::getWriteableConfig().setWifiMode(args.toInt(0));
                    config.write();
                    config.restartDevice();
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
                                args.printf_P(PSTR("enabled=%s"), debugLog.fullName());
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
                delay(100);
                panic();
            }
#endif
#if DEBUG_COLLECT_STRING_ENABLE
            else if (args.isCommand(PSTR("JSONSTRDUMP"))) {
                __debug_json_string_dump(output);
            }
#endif
            else {
                bool commandWasHandled = false;
                for(const auto plugin : plugins) { // send command to plugins
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
        }
    }
    if (nextCommand) {
        auto cmd = String(nextCommand);
        cmd.trim();
        if (cmd.length()) {
            at_mode_serial_handle_event(cmd);
        }
    }
}

void at_mode_serial_input_handler(Stream &client)
{
    static String line;

    if (System::Flags::getConfig().is_at_mode_enabled) {
        auto serial = StreamWrapper(serialHandler.getStreams(), serialHandler.getInput()); // local output online
        while(client.available()) {
            int ch = client.read();
            switch(ch) {
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
                    serial.write('\n');
                    at_mode_serial_handle_event(line);
                    line = String();
                    break;
                case '\r':
                    serial.write('\r');
                    break;
                default:
                    line += (char)ch;
                    serial.write(ch);
                    if (line.length() >= SERIAL_HANDLER_INPUT_BUFFER_MAX) {
                        serial.write('\n');
                        at_mode_serial_handle_event(line);
                        line = String();
                    }
                    break;
            }
        }
    }
}

#endif
