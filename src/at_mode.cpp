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
#include "at_mode.h"
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "fs_mapping.h"
#include "logger.h"
#include "misc.h"
#include "timezone.h"
#include "web_server.h"
#include "async_web_response.h"
#include "serial_handler.h"
#include "plugins.h"

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define AT_MODE_MAX_ARGUMENTS 16

static std::vector<const ATModeCommandHelp_t *> at_mode_help;

void at_mode_add_help(const ATModeCommandHelp_t *help) {
    at_mode_help.push_back(help);
}

void at_mode_display_help_indent(Stream &output, PGM_P text) {
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

void at_mode_display_help(Stream &output) {

    _debug_printf_P(PSTR("at_mode_display_help(): size=%d\n"), at_mode_help.size());

    for(const auto commandHelp: at_mode_help) {

        if (commandHelp->helpQueryMode) {
            if (commandHelp->command) {
                output.printf_P(PSTR(" AT+%s?\n"), commandHelp->command);
            } else {
                output.print(F(" AT?\n"));
            }
            at_mode_display_help_indent(output, commandHelp->helpQueryMode);
        }

        if (commandHelp->command) {
            output.printf_P(PSTR(" AT+%s"), commandHelp->command);
        } else {
            output.print(F(" AT"));
        }
        if (commandHelp->arguments) {
            PGM_P arguments = commandHelp->arguments;
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
        at_mode_display_help_indent(output, commandHelp->help);
    }
}

PROGMEM_AT_MODE_HELP_COMMAND_DEF_NNPP(AT, "Print OK", "Show help");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DSLP, "DSLP", "[<milliseconds>[,<mode>]]", "Enter deep sleep");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RST, "RST", "Soft reset");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CMDS, "CMDS", "Send a list of available AT commands");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(LOAD, "LOAD", "Discard changes and load settings from EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(STORE, "STORE", "Store current settings in EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FACTORY, "FACTORY", "Restore factory settings (but do not store in EEPROM)");

#if DEBUG

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PLUGINS, "PLUGINS", "List plugins");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
#if defined(ESP8266)
PROGMEM_AT_MODE_HELP_COMMAND_DEF(CPU, "CPU", "<80|160>", "Set CPU speed", "Display CPU speed");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMP, "DUMP", "Display settings");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPFS, "DUMPFS", "Display file system information");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DUMPEE, "DUMPEE", "[<offset>[,<length>]", "Dump EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WRTC, "WRTC", "<id,data>", "Write uint32 to RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DUMPRTC, "DUMPRTC", "Dump RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RTCCLR, "RTCCLR", "Clear RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RTCQCC, "RTCQCC", "<0=channel/bssid|1=static ip config.>", "Clear quick connect RTC memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIMO, "WIMO", "<0=off|1=STA|2=AP|3=STA+AP>", "Set WiFi mode, store configuration and reboot");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LOG, "LOG", "<message", "Send an error to the logger component");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PANIC, "PANIC", "Cause an exception by calling panic()");

#endif

void at_mode_help_commands() {

    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(AT));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DSLP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RST));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CMDS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOAD));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STORE));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FACTORY));

#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PLUGINS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HEAP));
#if defined(ESP8266)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CPU));
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPFS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPEE));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WRTC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPRTC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCCLR));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCQCC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIMO));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOG));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PANIC));
#endif

}

void at_mode_generate_help(Stream &output) {
    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto &plugin : plugins) {
        plugin.callAtModeCommandHandler(output, String(), AT_MODE_QUERY_COMMAND, nullptr);
    }
    at_mode_display_help(output);
    at_mode_help.clear();
}

String at_mode_print_command_string(Stream &output, char separator, bool trailingSeparator) {
    String commands;
    StreamString nullStream;

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto &plugin : plugins) {
        plugin.callAtModeCommandHandler(nullStream, String(), AT_MODE_QUERY_COMMAND, nullptr);
    }

    for(const auto commandHelp: at_mode_help) {
        if (commandHelp->command) {
            output.print(FPSTR(commandHelp->command));
            if (commandHelp == at_mode_help.back()) {
                if (trailingSeparator) {
                    output.print(separator);
                }
            } else {
                output.print(separator);
            }
        }
    }

    at_mode_help.clear();

    return commands;
}

#if DEBUG

static EventScheduler::TimerPtr heapTimer = nullptr;

static void heap_timer_callback(EventScheduler::TimerPtr timer) {
    MySerial.printf_P(PSTR("+HEAP: Free %u CPU %d MHz\n"), ESP.getFreeHeap(), ESP.getCpuFreqMHz());
}

static void create_heap_timer(int seconds) {
    heapTimer = Scheduler.addTimer(seconds * 1000, true, heap_timer_callback, EventScheduler::PRIO_LOW);
}

void at_mode_create_heap_timer(int seconds) {
    if (Scheduler.hasTimer(heapTimer)) {
        Scheduler.removeTimer(heapTimer);
    }
    if (seconds) {
        create_heap_timer(seconds);
    } else {
        heapTimer = nullptr;
    }
}

#endif

void at_mode_wifi_callback(uint8_t event, void *payload) {
    if (event == WiFiCallbacks::EventEnum_t::CONNECTED) {
        MySerial.printf_P(PSTR("WiFi connected to %s - IP %s\n"), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else if (event == WiFiCallbacks::EventEnum_t::DISCONNECTED) {
        MySerial.println(F("WiFi connection lost"));
    }
}

void at_mode_setup() {
    serialHandler.addHandler(at_mode_serial_input_handler, SerialHandler::RECEIVE|SerialHandler::REMOTE_RX);
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::DISCONNECTED, at_mode_wifi_callback);
}

void enable_at_mode() {
    auto &flags = config._H_W_GET(Config().flags);
    if (!flags.atModeEnabled) {
        _debug_println(F("Enabling AT MODE."));
        MySerial.println(F("Enabling AT MODE."));
        flags.atModeEnabled = true;
        // if (!heapTimer) {
        //     create_heap_timer(60);
        // }
    }
}

void disable_at_mode() {
    auto &flags = config._H_W_GET(Config().flags);
    if (flags.atModeEnabled) {
        _debug_println(F("Disabling AT MODE."));
        MySerial.println(F("Disabling AT MODE."));
        Scheduler.removeTimer(heapTimer);
        heapTimer = nullptr;
        flags.atModeEnabled = false;
    }
}

void at_mode_dump_fs_info(Stream &output) {
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

void at_mode_print_invalid_command(Stream &output) {
    output.println(F("ERROR - Invalid command. AT? for help"));
}

void at_mode_print_invalid_arguments(Stream &output) {
    output.println(F("ERROR - Invalid arguments. AT? for help"));
}

void at_mode_print_ok(Stream &output) {
    output.println(FSPGM(OK));
}

void at_mode_serial_handle_event(String &commandString) {
    auto &output = MySerial;
    commandString.trim();
#if AT_MODE_ALLOW_PLUS_WITHOUT_AT
    // allow using AT+COMMAND and +COMMAND
    if (!strncasecmp_P(commandString.c_str(), PSTR("AT"), 2)) {
        commandString.remove(0, 2);
    } else if (commandString.charAt(0) == '+') {
    } else {
        at_mode_print_invalid_command(output);
        return;
    }
#else
    if (!strncasecmp_P(commandString, PSTR("AT"))) {
        commandString.remove(0, 2);
    } else {
        at_mode_print_invalid_command(output);
        return;
    }
#endif
    if (commandString.length() == 0) { // AT
        at_mode_print_ok(output);
    } else {
#if defined(ESP8266)
        auto command = commandString.begin();
#else
        std::unique_ptr<char []> __command(new char[commandString.length() + 1]);
        auto command = strcpy(__command.get(), commandString.c_str());
#endif
        if (!strcmp_P(command, PSTR("?"))) { // AT?
            at_mode_generate_help(output);
        } else {

            int8_t argc;
            char *args[AT_MODE_MAX_ARGUMENTS];
            memset(args, 0, sizeof(args));

            // AT was remove already, string always starts with +
            command++;

            char *ptr = strchr(command, '?');
            if (ptr) { // query mode has no arguments
                *ptr = 0;
                argc = AT_MODE_QUERY_COMMAND;
            } else {
                _debug_printf_P(PSTR("tokenizer('%s')\n"), command);
                argc = (int8_t)tokenizer(command, args, AT_MODE_MAX_ARGUMENTS, true);
            }

            _debug_printf_P(PSTR("Command '%s' argc %d arguments '%s'\n"), command, argc, implode(F("','"), (const char **)args, argc).c_str());

            if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DSLP))) {
                uint32_t time = 0;
                RFMode mode = RF_DEFAULT;
                if (argc == 1) {
                    time = atoi(args[0]);
                }
                if (argc == 2) {
                    mode = (RFMode)atoi(args[1]);
                }
                output.println(F("Entering deep sleep..."));
                config.enterDeepSleep(time, mode, 1);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RST))) {
                output.println(F("Software reset..."));
                yield();
                delay(100);
                config.restartDevice();
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(CMDS))) {
                output.print(F("+CMDS="));
                at_mode_print_command_string(output, ',', false);
                output.println();
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(LOAD))) {
                config.read();
                at_mode_print_ok(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(STORE))) {
                config.write();
                at_mode_print_ok(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(FACTORY))) {
                config.restoreFactorySettings();
                at_mode_print_ok(output);
            }
#if DEBUG
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(PLUGINS))) {
                dump_plugin_list(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(HEAP))) {
                if (argc == 1) {
                    uint16_t interval = atoi(args[0]);
                    if (interval == 0) {
                        Scheduler.removeTimer(heapTimer);
                        heapTimer = nullptr;
                        output.println(F("+HEAP: Interval disabled"));
                    } else {
                        output.printf("+HEAP: Interval set to %d seconds\n", interval);
                        if (heapTimer) {
                            heapTimer->changeOptions(interval * 1000);
                        } else {
                            create_heap_timer(interval);
                        }
                    }
                } else {
                    at_mode_print_invalid_arguments(output);
                }
            }
#if defined(ESP8266)
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(CPU))) {
                if (argc == 1) {
                    uint8_t speed = atoi(args[0]);
                    bool result = system_update_cpu_freq(speed);
                    output.printf_P(PSTR("+CPU: Set %d MHz = %d\n"), speed, result);
                }
                output.printf_P(PSTR("+CPU: %d MHz\n"), ESP.getCpuFreqMHz());
            }
#endif
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DUMP))) {
                config.dump(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DUMPFS))) {
                at_mode_dump_fs_info(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DUMPEE))) {
                uint16_t offset = 0;
                uint16_t length = 0;
                if (argc > 0) {
                    offset = atoi(args[0]);
                    if (argc > 1) {
                        length = atoi(args[1]);
                    }
                }
                config.dumpEEPROM(output, false, offset, length);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(WRTC))) {
                if (argc < 2) {
                    at_mode_print_invalid_arguments(output);
                }
                else {
                    uint8_t rtcMemId = (uint8_t)atoi(args[0]);
                    uint32_t data = (uint32_t)atoi(args[1]);
                    RTCMemoryManager::write(rtcMemId, &data, sizeof(data));
                    output.printf_P(PSTR("+WRTC: id=%u, data=%u\n"), rtcMemId, data);
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DUMPRTC))) {
                RTCMemoryManager::dump(MySerial);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RTCCLR))) {
                RTCMemoryManager::clear();
                output.println(F("+RTCCLR: Memory cleared"));
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RTCQCC))) {
                if (argc == 1) {
                    int num = atoi(args[0]);
                    if (num == 0) {
                        uint8_t bssid[6];
                        memset(bssid, 0, sizeof(bssid));
                        config.storeQuickConnect(bssid, -1);
                        output.println(F("+RTCQCC: BSSID and channel removed"));
                    } else if (num == 1) {
                        config.storeStationConfig(0, 0, 0);
                        output.println(F("+RTCQCC: Static IP info removed"));
                    } else {
                        at_mode_print_invalid_arguments(output);
                    }
                } else {
                    at_mode_print_invalid_arguments(output);
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(WIMO))) {
                if (argc == 1) {
                    output.println(F("+WIFO: settings WiFi mode and restarting device..."));
                    config._H_W_GET(Config().flags).wifiMode = atoi(args[0]);
                    config.write();
                    config.restartDevice();
                } else {
                    at_mode_print_invalid_arguments(output);
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(LOG))) {
                if (argc < 1) {
                    at_mode_print_invalid_arguments(output);
                } else {
                    Logger_error(F("+LOG: %s"), implode(FSPGM(comma), (const char **)args, argc).c_str());
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(PANIC))) {
                output.println(F("Calling panic()"));
                delay(100);
                panic();
#endif
            } else {
                bool commandWasHandled = false;
                for(auto &plugin : plugins) { // send command to plugins
                    auto handler = plugin.getAtModeCommandHandler();
                    if (handler) {
                        if (true == (commandWasHandled = handler(MySerial, command, argc, args))) {
                            break;
                        }
                    }
                }
                if (!commandWasHandled) {
                    at_mode_print_invalid_command(MySerial);
                }
            }
        }
    }
}

String line_buffer;

void at_mode_serial_input_handler(uint8_t type, const uint8_t *buffer, size_t len) {

    Stream *echoStream = (type == SerialHandler::RECEIVE) ? &serialHandler.getSerial() : nullptr;
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

#endif
