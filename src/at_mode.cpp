/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <KFCSyslog.h>
#include <ProgmemStream.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <StreamString.h>
#include <vector>
#include <JsonTools.h>
#include <ListDir.h>
#include <Timezone.h>
#include "at_mode.h"
#include "kfc_fw_config.h"
#include "fs_mapping.h"
#include "logger.h"
#include "misc.h"
#include "web_server.h"
#include "async_web_response.h"
#include "serial_handler.h"
#include "blink_led_timer.h"
#include "pin_monitor.h"
#include "NeoPixel_esp.h"
#include "plugins.h"
#include "WebUIAlerts.h"
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
#include "plugins/dimmer_module/dimmer_base.h"
#endif

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

typedef std::vector<ATModeCommandHelp> ATModeHelpVector;

static ATModeHelpVector at_mode_help;

// class CatFile {
// public:
//     CatFile(const String &filename, Stream &output) : _output(output) {
//         _file = SPIFFS.open(filename, fs::FileOpenMode::read);
//     }
// private:
//     Stream &_output;
//     File _file;
//     EventScheduler _timer;
// };

void at_mode_add_help(const ATModeCommandHelp_t *help, PGM_P pluginName)
{
    at_mode_help.emplace_back(help, pluginName);
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
    _debug_printf_P(PSTR("size=%d, find=%s\n"), at_mode_help.size(), findText ? (findText->empty() ? PSTR("count=0") : implode(',', *findText).c_str()) : PSTR("nullptr"));
#endif
    if (findText && findText->empty()) {
        findText = nullptr;
    }
    for(const auto commandHelp: at_mode_help) {

        if (findText) {
            bool result = false;
            String tmp; // create single line text blob

            if (commandHelp.pluginName) {
                tmp += F("plugin "); // allows to search for "plugin sensor"
                _appendHelpString(tmp, commandHelp.pluginName);
            }
            if (commandHelp.commandPrefix && commandHelp.command) {
                String str(FPSTR(commandHelp.commandPrefix));
                str.toLowerCase();
                tmp += str;
            }
            _appendHelpString(tmp, commandHelp.command);
            _appendHelpString(tmp, commandHelp.arguments);
            _appendHelpString(tmp, commandHelp.help);
            _appendHelpString(tmp, commandHelp.helpQueryMode);

            _debug_printf_P(PSTR("find in %u: '%s'\n"), tmp.length(), tmp.c_str());
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

        if (commandHelp.helpQueryMode) {
            output.print(F(" AT"));
            if (commandHelp.command) {
                output.print('+');
                if (commandHelp.commandPrefix) {
                    output.print(FPSTR(commandHelp.commandPrefix));
                }
                output.print(FPSTR(commandHelp.command));
            }
            output.println('?');
            at_mode_display_help_indent(output, commandHelp.helpQueryMode);
        }

        output.print(F(" AT"));
        if (commandHelp.command) {
            output.print('+');
            if (commandHelp.commandPrefix) {
                output.print(FPSTR(commandHelp.commandPrefix));
            }
            output.print(FPSTR(commandHelp.command));
        }
        if (commandHelp.arguments) {
            PGM_P arguments = commandHelp.arguments;
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

        at_mode_display_help_indent(output, commandHelp.help);
    }
}

PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(AT, "Print OK", "Show help");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLP, "HLP", "[single][,word][,or entire phrase]", "Search help");
#if ENABLE_DEEP_SLEEP
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DSLP, "DSLP", "[<milliseconds>[,<mode>]]", "Enter deep sleep");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RST, "RST", "[<s>]", "Soft reset. 's' enables safe mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CMDS, "CMDS", "Send a list of available AT commands");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(LOAD, "LOAD", "Discard changes and load settings from EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(IMPORT, "IMPORT", "<filename|set_dirty>[,<handle>[,<handle>,...]]", "Import settings from JSON file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(STORE, "STORE", "Store current settings in EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FACTORY, "FACTORY", "Restore factory settings (but do not store in EEPROM)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ATMODE, "ATMODE", "<1|0>", "Enable/disable AT Mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DLY, "DLY", "<milliseconds>", "Call delay(milliseconds)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CAT, "CAT", "<filename>", "Display text file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RM, "RM", "<filename>", "Delete file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RN, "RN", "<filename>,<new filename>", "Rename file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LS, "LS", "[<directory>]", "List files and directory");
#if ESP8266
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LSR, "LSR", "[<directory>]", "List files and directory in raw mode");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "[<reset|on|off|ap_on|ap_off>]", "Modify WiFi settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(REM, "REM", "Ignore comment");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LED, "LED", "<slow,fast,flicker,off,solid,sos>,[,color=0xff0000][,pin]", "Set LED mode");
#if RTC_SUPPORT
PROGMEM_AT_MODE_HELP_COMMAND_DEF(RTC, "RTC", "[<set>]", "Set RTC time", "Display RTC time");
#endif

#if DEBUG

#if WEBUI_ALERTS_ENABLED || WEBUI_ALERTS_SEND_TO_LOGGER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ALERT, "ALERT", "<message>[,<type|0-3>]", "Add WebUI alert");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FSM, "FSM", "Display FS mapping");
#if PIN_MONITOR
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PINM, "PINM", "[<1=start|0=stop>}", "List or monitor PINs");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PLUGINS, "PLUGINS", "List plugins");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PLGI, "PLGI", "<name>[,1=end]", "Init or end plugin");
#if LOAD_STATISTICS
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap and system load");
#else
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RSSI, "RSSI", "[interval in seconds|0=disable]", "Display WiFi RSSI");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(GPIO, "GPIO", "[interval in seconds|0=disable]", "Display GPIO states");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PWM, "PWM", "<pin>,<level=0-1023/off>[,<frequency=1000Hz>]", "PWM output on PIN");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ADC, "ADC", "[interval in seconds|0=disable]", "Display ADC voltage");
#if defined(ESP8266)
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CPU, "CPU", "<80|160>", "Set CPU speed", "Display CPU speed");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMP, "DUMP", "[<dirty|config.name>]", "Display settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPR, "DUMPR", "<pointer>", "Print symbol");
#if DEBUG && ESP8266
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPT, "DUMPT", "Dump timers");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPFS, "DUMPFS", "Display file system information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPEE, "DUMPEE", "[<offset>[,<length>]", "Dump EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WRTC, "WRTC", "<id,data>", "Write uint32 to RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPRTC, "DUMPRTC", "Dump RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RTCCLR, "RTCCLR", "Clear RTC memory");
#if ENABLE_DEEP_SLEEP
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RTCQCC, "RTCQCC", "<0=channel/bssid|1=static ip config.>", "Clear quick connect RTC memory");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIMO, "WIMO", "<0=off|1=STA|2=AP|3=STA+AP>", "Set WiFi mode, store configuration and reboot");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOG, "LOG", "<message>", "Send an error to the logger component");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOGE, "LOGE", "<debug=enable/disable>", "Enable/disable writing to file log://debug");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PANIC, "PANIC", "Cause an exception by calling panic()");

#endif

void at_mode_help_commands()
{
    auto name = PSTR("at_mode");
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(AT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLP), name);
#if ENABLE_DEEP_SLEEP
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DSLP), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RST), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CMDS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOAD), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(IMPORT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STORE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FACTORY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(ATMODE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DLY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CAT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RN), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LS), name);
#if ESP8266
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LSR), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIFI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LED), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(REM), name);
#if RTC_SUPPORT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTC), name);
#endif

#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FSM), name);
#if PIN_MONITOR
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PINM), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PLUGINS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PLGI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HEAP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RSSI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(GPIO), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PWM), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(ADC), name);
#if defined(ESP8266)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CPU), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPR), name);
#if DEBUG && ESP8266
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPT), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPFS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPEE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WRTC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPRTC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCCLR), name);
#if ENABLE_DEEP_SLEEP
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCQCC), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIMO), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOG), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOGE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PANIC), name);
#endif

}

void at_mode_generate_help(Stream &output, StringVector *findText = nullptr)
{
    _debug_printf_P(PSTR("find=%s\n"), findText ? implode(',', *findText).c_str() : PSTR("nullptr"));
    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin: plugins) {
        plugin->atModeHelpGenerator();
    }
    at_mode_display_help(output, findText);
    at_mode_help.clear();
    if (config.isSafeMode()) {
        output.printf_P(PSTR("\nSAFE MODE ENABLED\n\n"));
    }
}

String at_mode_print_command_string(Stream &output, char separator)
{
    String commands;
    StreamString nullStream;

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin : plugins) {
        plugin->atModeHelpGenerator();
    }

    uint16_t count = 0;
    for(const auto commandHelp: at_mode_help) {
        if (commandHelp.command) {
            if (count++ != 0) {
                output.print(separator);
            }
            if (commandHelp.commandPrefix) {
                output.print(FPSTR(commandHelp.commandPrefix));
            }
            output.print(FPSTR(commandHelp.command));
        }
    }

    at_mode_help.clear();

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

    void removeTimer() {
        _timer.remove();
    }

public:
    EventScheduler::Timer _timer;
    DisplayTypeEnum_t _type = HEAP;
    int32_t _rssiMin, _rssiMax;
};

DisplayTimer displayTimer;

static void heap_timer_callback(EventScheduler::TimerPtr timer)
{
    if (displayTimer._type == DisplayTimer::HEAP) {
        MySerial.printf_P(PSTR("+HEAP: Free %u CPU %d MHz"), ESP.getFreeHeap(), ESP.getCpuFreqMHz());
#if LOAD_STATISTICS
        MySerial.printf_P(PSTR(" load avg %.2f %.2f %.2f"), LOOP_COUNTER_LOAD(load_avg[0]), LOOP_COUNTER_LOAD(load_avg[1]), LOOP_COUNTER_LOAD(load_avg[2]));
#endif
        MySerial.println();
    }
    else if (displayTimer._type == DisplayTimer::GPIO) {
        MySerial.printf_P(PSTR("+GPIO: "));
#if defined(ESP8266)
        for(uint8_t i = 0; i <= 16; i++) {
            if (i != 1 && i != 3 && !isFlashInterfacePin(i)) { // do not display RX/TX and flash SPI
                // pinMode(i, INPUT);
                MySerial.printf_P(PSTR("%u=%u "), i, digitalRead(i));
            }
        }
        // pinMode(A0, INPUT);
        MySerial.printf_P(PSTR(" A0=%u\n"), analogRead(A0));
#else
// #warning not implemented
#endif
    }
    else {
        int32_t rssi = WiFi.RSSI();
        displayTimer._rssiMin = std::max(displayTimer._rssiMin, rssi);
        displayTimer._rssiMax = std::min(displayTimer._rssiMax, rssi);
        MySerial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, displayTimer._rssiMin, displayTimer._rssiMax);
    }
}

static void create_heap_timer(float seconds, DisplayTimer::DisplayTypeEnum_t type = DisplayTimer::HEAP)
{
    displayTimer._type = type;
    if (displayTimer._timer.active()) {
        displayTimer._timer->rearm(seconds * 1000);
    } else {
        displayTimer._timer.add(seconds * 1000, true, heap_timer_callback, EventScheduler::PRIO_LOW);
    }
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

void at_mode_wifi_callback(uint8_t event, void *payload)
{
    if (event == WiFiCallbacks::EventEnum_t::CONNECTED) {
        MySerial.printf_P(PSTR("WiFi connected to %s - IP %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else if (event == WiFiCallbacks::EventEnum_t::DISCONNECTED) {
        MySerial.println(F("WiFi connection lost"));
    }
}

PROGMEM_STRING_DEF(atmode_file_autorun, "/autorun_atmode.cmd");
void at_mode_serial_handle_event(String &commandString);

static void at_mode_autorun()
{
    LoopFunctions::callOnce([]() {
        File file = SPIFFS.open(FSPGM(atmode_file_autorun), FileOpenMode::read);
        _debug_printf_P(PSTR("autorun=%s size=%u\n"), SPGM(atmode_file_autorun), file.size());
        if (file) {
            class CmdAt {
            public:
                CmdAt(uint32_t time, const String &command) : _time(time), _command(command) {
                }
                uint32_t _time;
                String _command;
            };
            typedef std::vector<CmdAt> CmdAtVector;
            CmdAtVector *commands = new CmdAtVector();

            String cmd;
            uint32_t atTime = 0, startTime = millis() + 50;
            while(file.available()) {
                cmd = file.readStringUntil('\n');
                cmd.replace(F("\r"), emptyString);
                cmd.replace(F("\n"), emptyString);
                cmd.trim();
                if (cmd.length()) {
                    auto ptr = cmd.c_str();
                    if (*ptr++ == '@') {
                        if (*ptr == '+') {
                            ptr++;
                        }
                        else {
                            atTime = startTime;
                        }
                        atTime += atoi(ptr);
                    }
                    else {
                        if (atTime) {
                            commands->emplace_back(atTime, cmd);
                        }
                        else {
                            at_mode_serial_handle_event(cmd);
                        }
                    }
                }
            }

            if (commands->size()) {

                Scheduler.addTimer(50, true, [commands](EventScheduler::TimerPtr timer) {
                    uint32_t minCmdTime = UINT32_MAX;
                    for(auto &cmd: *commands) {
                        if (cmd._command.length()) {
                            if (millis() >= cmd._time) {
                                // debug_printf_P(PSTR("scheduled=%u cmd=%s\n"), cmd._time, cmd._command.c_str());
                                at_mode_serial_handle_event(cmd._command);
                                cmd._command = String();
                            }
                            else {
                                minCmdTime = std::min(cmd._time, minCmdTime);
                            }
                        }
                    }
                    commands->erase(std::remove_if(commands->begin(), commands->end(), [](const CmdAt &cmd) {
                        return cmd._command.length() == 0;
                    }));
                    if (!commands->size()) {
                        timer->detach();
                    }
                    else {
                        minCmdTime = std::max(minCmdTime, (uint32_t)millis() + 50) - millis();
                        timer->rearm(minCmdTime, true);
                    }

                }, EventScheduler::PRIO_NORMAL, [commands](EventTimer *ptr) {
                    debug_printf_P(PSTR("deleter for scheduled at commands called %p, commands %p\n"), ptr, commands);
                    delete commands;
                    delete ptr;
                });

            }
            else {
                delete commands;
            }
        }
    });
}

void at_mode_setup()
{
    SerialHandler::getInstance().addHandler(at_mode_serial_input_handler, SerialHandler::RECEIVE|SerialHandler::REMOTE_RX);
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::DISCONNECTED, at_mode_wifi_callback);

    if (!config.isSafeMode()) {
        at_mode_autorun();
    }
}

void enable_at_mode(Stream &output)
{
    auto flags = config._H_GET(Config().flags);
    if (!flags.atModeEnabled) {
        output.println(F("Enabling AT MODE."));
        flags.atModeEnabled = true;
        config._H_SET(Config().flags, flags);
    }
}

void disable_at_mode(Stream &output)
{
    auto flags = config._H_GET(Config().flags);
    if (flags.atModeEnabled) {
        output.println(F("Disabling AT MODE."));
#if DEBUG
        displayTimer.removeTimer();
#endif
        flags.atModeEnabled = false;
        config._H_SET(Config().flags, flags);
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

void at_mode_print_invalid_command(Stream &output)
{
    output.print(F("ERROR - Invalid command. "));
    if (config.isSafeMode()) {
        output.print(F(""));
    }
    output.println(F("AT? for help"));
}

void at_mode_print_invalid_arguments(Stream &output, uint16_t num, uint16_t min, uint16_t max)
{
    static const uint16_t UNSET = ~0;
    output.print(F("ERROR - "));
    if (min != UNSET) {
        if (min == max || max == UNSET) {
            output.printf_P(PSTR("Expected %u argument(s), got %u. AT? for help\n"), min, num);
        }
        else {
            output.printf_P(PSTR("Expected %u to %u argument(s), got %u. AT? for help\n"), min, max, num);
        }
    }
    else {
        output.println(F("Invalid arguments. AT? for help"));
    }
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

#if DEBUG

#include <map>
#include "RestApi/RetrieveSymbols.h"

static std::map<void *, String> at_mode_resolve_map;

void *resolve_lambda(void *ptr)
{
    at_mode_resolve(ptr, [](const String &symbol) {
        MySerial.printf_P(PSTR("lambda: %s\n"), symbol.c_str());
    });
    return ptr;
}

#endif

void at_mode_resolve(void *ptr, AtModeResolveACallback resolve_callback)
{
#if 0
    auto iterator = at_mode_resolve_map.find(ptr);
    if (iterator != at_mode_resolve_map.end()) {
        resolve_callback(iterator->second);
    }
    else {
        if (ptr) {
            StringVector addresses;
            addresses.push_back(PrintString(F("0x%x"), ptr));
            auto rs = new RetrieveSymbols::RestApi();
            rs->setAddresses(std::move(addresses));
            int retries = 0;
            rs->call([rs, resolve_callback, ptr, retries](RetrieveSymbols::JsonReaderResult *result, const String &error) mutable {
                rs->setAutoDelete(true);
                if (result) {
                    PrintString str;
                    int num = 1;
                    for (const auto &item : result->getItems()) {
                        if (str.length()) {
                            str.println();
                        }
                        str.printf_P(PSTR("%u 0x%08x 0x%08x:0x%04x %s"), num++, item.getSrcAddress(), item.getAddress(), item.getSize(), item.getName().c_str());
                    }
                    at_mode_resolve_map[ptr] = str;
                }
                else {
                    if (++retries <= 3 && false) {
                        debug_printf_P(PSTR("retries=%d\n"), retries);
                        rs->setAutoDelete(false);
                        Scheduler.addTimer(5000, false, [rs](EventScheduler::TimerPtr) {
                            rs->call(rs->_callback);
                        });
                    }
                    at_mode_resolve_map[ptr] = PrintString(F("%p - %s"), ptr, error.c_str());
                }
                resolve_callback(at_mode_resolve_map[ptr]);
            });

        }
        else {
            at_mode_resolve_map[ptr] = SPGM(null);
            resolve_callback(at_mode_resolve_map[ptr]);
        }
    }
#endif
}

#if DEBUG && ESP8266

extern ETSTimer *timer_list;

void at_mode_list_ets_timers(Print &output)
{
    StringVector addresses;
    ETSTimer *cur = timer_list;
    while(cur) {
        for(const auto &timer: Scheduler._timers) {
            if (&timer->_etsTimer == cur) {
                addresses.push_back(PrintString(F("0x%x"), lambda_target(timer->_loopCallback)));
                addresses.push_back(PrintString(F("0x%x"), (uint32_t)cur->timer_func));
                addresses.push_back(PrintString(F("0x%x"), (uint32_t)cur->timer_arg));
                break;
            }
        }
        // float period_in_s = cur->timer_period / 312500.0;
        // output.printf_P(PSTR("ETSTimer=%p func=%p arg=%p period=%u (%.3fs) exp=%u callback=%p\n"), cur, cur->timer_func, cur->timer_arg, cur->timer_period, period_in_s, cur->timer_expire, callback);
        cur = cur->timer_next;
    }

    output.println(F("Waiting for symbols..."));
    xtra_containers::remove_duplicates(addresses);
    auto rs = new RetrieveSymbols::RestApi();
    rs->setAutoDelete(true);
    rs->setAddresses(std::move(addresses));
    rs->call([&output](RetrieveSymbols::JsonReaderResult *result, const String &error) {
        if (result) {
            auto &items = result->getItems();
            auto getAddr = [&items](uint32_t addr) {
                auto it = std::find(items.begin(), items.end(), addr);
                if (it == items.end()) {
                    return String('-');
                }
                auto name = it->getName();
                if (name.length() > 64) {
                    name.remove(60, -1);
                    name += F("...#");
                    name += String(std::distance(items.begin(), it));
                }
                return name;
            };
            ETSTimer *cur = timer_list;
            while(cur) {
                void *callback = nullptr;
                for(const auto &timer: Scheduler._timers) {
                    if (&timer->_etsTimer == cur) {
                        callback = lambda_target(timer->_loopCallback);
                        break;
                    }
                }
                float period_in_s = cur->timer_period / 312500.0;
                output.printf_P(PSTR("ETSTimer=%p func=%p[%s] arg=%p[%s] period=%u (%.3fs) exp=%u callback=%p[%s]\n"),
                    cur,
                    cur->timer_func, getAddr((uint32_t)cur->timer_func).c_str(),
                    cur->timer_arg, getAddr((uint32_t)cur->timer_arg).c_str(),
                    cur->timer_period,
                    period_in_s,
                    cur->timer_expire,
                    callback, getAddr((uint32_t)callback).c_str());
                cur = cur->timer_next;
            }
            size_t num = 0;
            for (auto item : result->getItems()) {
                output.printf_P(PSTR("#% 2u 0x%08x 0x%08x:0x%04x %s\n"), num++, item.getSrcAddress(), item.getAddress(), (uint32_t)item.getSize(), item.getName().c_str());
            }
        }
        else {
            output.println(error);
            ETSTimer *cur = timer_list;
            while(cur) {
                void *callback = nullptr;
                for(const auto &timer: Scheduler._timers) {
                    if (&timer->_etsTimer == cur) {
                        callback = lambda_target(timer->_loopCallback);
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

        }
    });
}

#endif

void at_mode_serial_handle_event(String &commandString)
{
    auto &output = MySerial;
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
                _debug_printf_P(PSTR("tokenizer('%s')\n"), command);
                args.setQueryMode(false);
                tokenizer(command, args, true, &nextCommand);
            }
            _debug_printf_P(PSTR("cmd=%s,argc=%d,args='%s',next_cmd='%s'\n"), command, args.size(), implode(F("','"), args.getArgs()).c_str(), nextCommand ? nextCommand : SPGM(0, "0"));

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
            if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLP))) {
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
                        args.print(F("configuration marked dirty"));
                    }
                    else {
                        auto file = SPIFFS.open(filename, fs::FileOpenMode::read);
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
            else if (args.isCommand(F("I2CT")) || args.isCommand(F("I2CR"))) {
                // ignore SerialTwoWire communication
            }
#if 0
            else if (args.isCommand(PSTR("DIMTEST"))) {
                static EventScheduler::TimerPtr timer = nullptr;
                static int channel, value;
                value = 500;
                channel = argc == 1 ? atoi(args[0]) : 0;
                MySerial.printf_P(PSTR("+i2ct=17,82,ff,00,00,00,00,00,00,10\n"));
                Scheduler.removeTimer(timer);
                Scheduler.addTimer(&timer, 4000, true, [&output](EventScheduler::TimerPtr timer) {
                    value += 50;
                    if (value > 8333) {
                        value = 8333;
                        timer->detach();
                    }
                    output.printf_P(PSTR("VALUE %u\n"), value);
                    MySerial.printf_P(PSTR("+i2ct=17,82,%02x,%02x,%02x,00,00,00,00,10\n"), channel&0xff, lowByte(value)&0xff, highByte(value)&0xff);
                });
            }
#endif
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

                using KFCConfigurationClasses::MainConfig;

                auto flags = config._H_GET(Config().flags);
                args.printf_P("station mode %s, DHCP %s, SSID %s, connected %s, IP %s",
                    (WiFi.getMode() & WIFI_STA) ? SPGM(on) : SPGM(off),
                    flags.stationModeDHCPEnabled ? SPGM(on) : SPGM(off),
                    config._H_STR(MainConfig().network.WiFiConfig._ssid),
                    WiFi.isConnected() ? SPGM(yes) : SPGM(no),
                    WiFi.localIP().toString().c_str()
                );
                args.printf_P("AP mode %s, DHCP %s, SSID %s, clients connected %u, IP %s",
                    (flags.wifiMode & WIFI_AP) ? ((flags.apStandByMode) ? ((WiFi.getMode() & WIFI_AP) ? SPGM(on) : PSTR("stand-by")) : SPGM(on)) : SPGM(off),
                    flags.softAPDHCPDEnabled ? SPGM(on) : SPGM(off),
                    config._H_STR(MainConfig().network.WiFiConfig._softApSsid),
                    WiFi.softAPgetStationNum(),
                    config._H_GET(MainConfig().network.softAp).address().toString().c_str()
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
                    auto result = SPIFFS.remove(filename);
                    args.printf_P(PSTR("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RN))) {
                if (args.requireArgs(2, 2)) {
                    auto filename = args.get(0);
                    auto newFilename = args.get(0);
                    auto result = SPIFFS.rename(filename, newFilename);
                    args.printf_P(PSTR("%s => %s: %s"), filename, newFilename, result ? PSTR("success") : PSTR("failure"));
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LS))) {
                auto dir = ListDir(args.toString(0));
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
                auto dir = SPIFFS.openDir(args.toString(0));
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
                    auto filename = args.get(0);
                    auto file = SPIFFSWrapper::open(filename, fs::FileOpenMode::read);
                    if (file) {
                        Scheduler.addTimer(10, true, [&output, &args, file](EventScheduler::TimerPtr timer) mutable {
                            char buf[256];
                            if (file.available()) {
                                auto len = file.readBytes(buf, sizeof(buf) - 1);
                                if (len) {
                                    buf[len] = 0;
                                    output.print(buf);
                                    output.flush();
                                }
                            }
                            else {
                                args.printf_P(PSTR("%s: %u"), file.name(), (unsigned)file.size());
                                file.close();
                                timer->detach();
                            }
                        });
                    }
                    else {
                        args.printf_P(PSTR("Failed to open: %s"), filename);
                    }
                }
            }
#if DEBUG
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
                // Mappings::getInstance().dump(output);
            }
    #if WEBUI_ALERTS_ENABLED || WEBUI_ALERTS_SEND_TO_LOGGER
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ALERT))) {
                if (args.size() > 0) {
                    WebUIAlerts_add(args.get(0), static_cast<AlertMessage::TypeEnum_t>(args.toInt(1, 0)));
                    args.print(F("Alert added"));
                }
                #if WEBUI_ALERTS_ENABLED
                for(auto &alert: config.getAlerts()) {
                    String str;
                    KFCFWConfiguration::AlertMessage::toString(str, alert);
                    String_rtrim_P(str, PSTR("\r\n"));
                    args.print(str.c_str());
                }
                #endif
            }
    #endif
    #if PIN_MONITOR
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PINM))) {
                if (!PinMonitor::getInstance()) {
                    args.print(F("Pin monitor not initialized"));
                }
                else if (args.size() == 1) {
                    static EventScheduler::Timer timer;
                    if (args.isTrue(0) && !timer.active()) {
                        args.print(F("Started"));
                        timer.add(1000, true, [&output](EventScheduler::TimerPtr timer) {
                            PinMonitor::getInstance()->dumpPins(output);
                        });
                    }
                    else if (args.isFalse(0) && timer.active()) {
                        timer.remove();
                        args.print(F("Stopped"));
                    }
                } else {
                    PinMonitor::getInstance()->dumpPins(output);
                }
            }
    #endif
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PLUGINS))) {
                dump_plugin_list(output);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PLGI))) {
                if (args.requireArgs(1, 2)) {
                    auto name = args.toString(0);
                    auto end = args.isTrue(1);
                    PluginComponent *plugin = nullptr;
                    for(const auto plugin2 : plugins) {
                        if (name.equalsIgnoreCase(FPSTR(plugin2->getName()))) {
                            plugin = plugin2;
                            break;
                        }
                    }
                    if (plugin) {
                        if (end) {
                            args.printf_P(PSTR("Calling %s.shutdown()"), name.c_str());
                            plugin->shutdown();
                        }
                        else {
                            args.printf_P(PSTR("Calling %s.setup()"), name.c_str());
                            plugin->setup(PluginComponent::PLUGIN_SETUP_DEFAULT);
                        }
                    }
                    else {
                        args.printf_P(PSTR("Cannot find plugin '%s'"), name.c_str());
                    }

                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                if (args.requireArgs(1, 1)) {
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
                        displayTimer.removeTimer();
                        args.print(F("Interval disabled"));
                    } else {
                        float fInterval = interval / 1000.0;
                        args.printf_P(PSTR("Interval set to %.3f seconds"), fInterval);
                        create_heap_timer(fInterval, displayTimer._type);
                    }
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PWM))) {
                if (args.requireArgs(2, 3)) {
                    auto pin = (uint8_t)args.toInt(0);
                    if (args.equalsIgnoreCase(1, F("off"))) {
                        pinMode(pin, INPUT);
                        args.printf_P(PSTR("set pin=%u to INPUT"), pin);
                    }
                    else {
                        auto level = (uint16_t)args.toInt(1, 0);
                        auto freq = (uint16_t)args.toInt(2, 1000);
                        pinMode(pin, OUTPUT);
#if ESP266
                        analogWriteFreq(freq);
#endif
                        analogWrite(pin, level * 1.02); // this might be different, just tested with a single ESP12F
                        double dc = (1000000 / (double)freq) * (level / 1024.0);
                        args.printf_P(PSTR("set pin=%u to OUTPUT, level=%u (%.2fµs), f=%uHz"), pin, level, dc, freq);
                    }
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ADC))) {
                static EventScheduler::Timer adcTimer;
                if (args.requireArgs(1)) {
                    if (args.equalsIgnoreCase(1, F("off"))) {
                        args.print(F("ADC display off"));
                        adcTimer.remove();
                    }
                    else {
                        auto interval = (uint16_t)args.toMillis(0, 100, ~0, 1000, String('s'));
                        args.printf_P(PSTR("ADC interval %ums"), interval);
                        auto &stream = args.getStream();
                        adcTimer.add(interval, true, [&stream](EventScheduler::TimerPtr) {
                            auto value = analogRead(A0);
                            stream.printf_P(PSTR("+ADC: %u - %.3fV\n"), value, value / 1024.0);
                        });
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPR))) {
                if (args.requireArgs(1, 1)) {
                    output.println(F("Waiting for symbols..."));
                    at_mode_resolve((void *)args.toNumber(0), [args](const String &symbol) mutable {
                        args.printf_P(PSTR("%s"), symbol.c_str());
                    });
                }
            }
#if DEBUG && ESP8266
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPT))) {
                at_mode_list_ets_timers(output);
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
                    uint8_t rtcMemId = (uint8_t)args.toInt(0);
                    uint32_t data = (uint32_t)args.toNumber(1);
                    RTCMemoryManager::write(rtcMemId, &data, sizeof(data));
                    args.printf_P(PSTR("id=%u, data=%u (%x)"), rtcMemId, data, data);
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DUMPRTC))) {
                RTCMemoryManager::dump(MySerial);
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
                    auto flags = config._H_GET(Config().flags);
                    flags.wifiMode = args.toInt(0);
                    config._H_SET(Config().flags, flags);
                    config.write();
                    config.restartDevice();
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOG))) {
                if (args.requireArgs(1)) {
                    Logger_error(F("+LOG: %s"), implode(',', args.getArgs()).c_str());
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOGE))) {
                if (args.requireArgs(1)) {
                    bool enable = args.isTrue(0);
                    _logger.setLogLevel(enable ? LOGLEVEL_DEBUG : LOGLEVEL_ACCESS);
                    args.printf_P(PSTR("debug=%u"), enable);
                }
            }
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


void at_mode_serial_input_handler(uint8_t type, const uint8_t *buffer, size_t len)
{
    static String line_buffer;

    Stream *echoStream = (type == SerialHandler::RECEIVE) ? &SerialHandler::getInstance().getSerial() : nullptr;
    if (config._H_GET(Config().flags).atModeEnabled) {
        char *ptr = (char *)buffer;
        while(len--) {
            if (*ptr == 9) {
                if (line_buffer.length() == 0) {
                    line_buffer = F("AT+");
                    if (echoStream) {
                        echoStream->print(line_buffer);
                    }
                }
            } else if (*ptr == 8) {
                if (line_buffer.length()) {
                    line_buffer.remove(line_buffer.length() - 1, 1);
                }
                if (echoStream) {
                    echoStream->print(F("\b \b"));
                }
            } else if (*ptr == '\n') {
                if (echoStream) {
                    echoStream->print('\n');
                }
                at_mode_serial_handle_event(line_buffer);
                line_buffer = String();
            } else if (*ptr == '\r') {
                if (echoStream) {
                    echoStream->print('\r');
                }
            } else {
                line_buffer += (char)*ptr;
                if (echoStream) {
                    echoStream->print((char)*ptr);
                }
                if (line_buffer.length() >= SERIAL_HANDLER_INPUT_BUFFER_MAX) {
                    if (echoStream) {
                        echoStream->print('\n');
                    }
                    at_mode_serial_handle_event(line_buffer);
                    line_buffer = String();
                }
            }
            ptr++;
        }
    }
}

#else

void *resolve_lambda(void *ptr) {
    return ptr;
}

#endif
