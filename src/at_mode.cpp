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
#include "pin_monitor.h"
#include "plugins.h"
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
#include "plugins/dimmer_module/dimmer_base.h"
#endif

#if DEBUG_AT_MODE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define AT_MODE_MAX_ARGUMENTS 16

typedef std::vector<const ATModeCommandHelp_t *> ATModeHelpVector;

static ATModeHelpVector at_mode_help;

// class CatFile {
// public:
//     CatFile(const String &filename, Stream &output) : _output(output) {
//         _file = SPIFFS.open(filename, "r");
//     }
// private:
//     Stream &_output;
//     File _file;
//     EventScheduler _timer;
// };

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
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RST, "RST", "[<s>]", "Soft reset. 's' enables safe mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(CMDS, "CMDS", "Send a list of available AT commands");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(LOAD, "LOAD", "Discard changes and load settings from EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(IMPORT, "IMPORT", "<filename>[,<handle>[,<handle>,...]]", "Import settings from JSON file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(STORE, "STORE", "Store current settings in EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FACTORY, "FACTORY", "Restore factory settings (but do not store in EEPROM)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ATMODE, "ATMODE", "<1|0>", "Enable/disable AT Mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DLY, "DLY", "<milliseconds>", "Call delay(milliseconds)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(CAT, "CAT", "<filename>", "Display text file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DEL, "DEL", "<filename>", "Delete file");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "[<reconnect>]", "Display WiFi info");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(REM, "REM", "Ignore comment");
#if RTC_SUPPORT
PROGMEM_AT_MODE_HELP_COMMAND_DEF(RTC, "RTC", "[<set>]", "Set RTC time", "Display RTC time");
#endif

#if DEBUG_HAVE_SAVECRASH
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHC, "SAVECRASHC", "Clear crash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHP, "SAVECRASHP", "rint saved crash details");
#endif

#if DEBUG

#if PIN_MONITOR
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PINM, "PINM", "[<1=start|0=stop>}", "List or monitor PINs");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(PLUGINS, "PLUGINS", "List plugins");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HEAP, "HEAP", "[interval in seconds|0=disable]", "Display free heap");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RSSI, "RSSI", "[interval in seconds|0=disable]", "Display WiFi RSSI");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(GPIO, "GPIO", "[interval in seconds|0=disable]", "Display GPIO states");
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
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(IMPORT));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STORE));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FACTORY));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(ATMODE));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DLY));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CAT));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DEL));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIFI));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(REM));
#if RTC_SUPPORT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTC));
#endif

#if DEBUG_HAVE_SAVECRASH
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHP));
#endif

#if DEBUG
#if PIN_MONITOR
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PINM));
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PLUGINS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HEAP));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RSSI));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(GPIO));
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
    for(auto plugin : plugins) {
        plugin->atModeHelpGenerator();
    }
    at_mode_display_help(output);
    at_mode_help.clear();
}

String at_mode_print_command_string(Stream &output, char separator) {
    String commands;
    StreamString nullStream;

    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin : plugins) {
        plugin->atModeHelpGenerator();
    }

    uint16_t count = 0;
    for(const auto commandHelp: at_mode_help) {
        if (commandHelp->command) {
            if (count++ != 0) {
                output.print(separator);
            }
            output.print(FPSTR(commandHelp->command));
        }
    }

    at_mode_help.clear();

    return commands;
}

#if DEBUG

typedef enum {
    HEAP = 1,
    RSSI = 2,
    GPIO = 3,
} DisplayTypeEnum_t;

static EventScheduler::TimerPtr heapTimer = nullptr;
static DisplayTypeEnum_t displayType = HEAP;
static int32_t rssiMin, rssiMax;

static void heap_timer_callback(EventScheduler::TimerPtr timer) {
    if (displayType == HEAP) {
        MySerial.printf_P(PSTR("+HEAP: Free %u CPU %d MHz\n"), ESP.getFreeHeap(), ESP.getCpuFreqMHz());
    }
    else if (displayType == GPIO) {
        MySerial.printf_P(PSTR("+GPIO: "));
#if defined(ESP8266)
        for(uint8_t i = 0; i <= 16; i++) {
            if (i != 1 && i != 3 && !isFlashInterfacePin(i)) { // do not display RX/TX and flash SPI
                pinMode(i, INPUT);
                MySerial.printf_P(PSTR("%u=%u "), i, digitalRead(i));
            }
        }
        pinMode(A0, INPUT);
        MySerial.printf_P(PSTR(" A0=%u\n"), analogRead(A0));
#else
#error not implemented
#endif
    }
    else {
        auto rssi = WiFi.RSSI();
        rssiMin = std::max(rssiMin, rssi);
        rssiMax = std::min(rssiMax, rssi);
        MySerial.printf_P(PSTR("+RSSI: %d dBm (min/max %d/%d)\n"), rssi, rssiMin, rssiMax);
    }
}

static void create_heap_timer(float seconds, DisplayTypeEnum_t type = HEAP)
{
    displayType = type;
    if (Scheduler.hasTimer(heapTimer)) {
        heapTimer->changeOptions(seconds * 1000);
    } else {
        Scheduler.addTimer(&heapTimer, seconds * 1000, true, heap_timer_callback, EventScheduler::PRIO_LOW);
    }
}

void at_mode_create_heap_timer(float seconds) {
    if (seconds) {
        create_heap_timer(seconds, HEAP);
    } else {
        if (Scheduler.hasTimer(heapTimer)) {
            Scheduler.removeTimer(heapTimer);
        }
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
    SerialHandler::getInstance().addHandler(at_mode_serial_input_handler, SerialHandler::RECEIVE|SerialHandler::REMOTE_RX);
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::DISCONNECTED, at_mode_wifi_callback);
}

void enable_at_mode(Stream &output) {
    auto &flags = config._H_W_GET(Config().flags);
    if (!flags.atModeEnabled) {
        output.println(F("Enabling AT MODE."));
        flags.atModeEnabled = true;
    }
}

void disable_at_mode(Stream &output) {
    auto &flags = config._H_W_GET(Config().flags);
    if (flags.atModeEnabled) {
        output.println(F("Disabling AT MODE."));
#if DEBUG
        Scheduler.removeTimer(heapTimer);
        heapTimer = nullptr;
#endif
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

/*


bool read_response(Buffer &buffer, size_t size, int timeout = 500) {
    unsigned long end = millis() + timeout;
    debug_output.print(F("Resp: "));
    while(millis() < end) {
        while(MySerial.available()) {
            auto ch = MySerial.read();
            buffer.write(ch);
            debug_output.printf_P(PSTR("%02x "), ch);
            if (buffer.length() == size) {
                debug_output.println();
                return true;
            }
        }
        delay(1);
    }
    debug_output.println(F("timeout"));
    return false;
}


bool sync() {
    Buffer buffer;
    const char cmd[] = { Cmnd_STK_GET_SYNC, Sync_CRC_EOP };
    const char rsp[] = { Resp_STK_INSYNC, Resp_STK_OK };

    MySerial.write(cmd, sizeof(cmd));

    for(uint8_t i = 0; i < 2; i++) {
        MySerial.write(cmd, sizeof(cmd));


        buffer.clear();
        if (read_response(buffer, sizeof(rsp))) {
            if (strncmp(buffer.getConstChar(), rsp, sizeof(rsp)) == 0) {
                return true;
            }
        }

        debug_output.printf_P(PSTR("Sync failed %u, response len %u\n"), i + 1, buffer.length());
    }
    return false;
}

bool verify_signature() {
    Buffer buffer;
    const char cmd[] = { Cmnd_STK_READ_SIGN, Sync_CRC_EOP };
    const char rsp[] = { Resp_STK_INSYNC, Resp_STK_OK };

    MySerial.write(cmd, sizeof(cmd));


}

void flash() {


    // clean buffer
    while (MySerial.available()) {
        MySerial.read();
    }

    // sync with atmega
    if (sync()) {

    }


    MySerial.println(debug_output);
    debug_output = PrintString();
}
*/

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
        at_mode_print_ok(output);
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
            int8_t argc;
            char *args[AT_MODE_MAX_ARGUMENTS];
            memset(args, 0, sizeof(args));

            // AT was remove already, string always starts with +
            command++;

            char *ptr = strchr(command, '?');
            if (isQueryMode && ptr) { // query mode has no arguments
                *ptr = 0;
                argc = AT_MODE_QUERY_COMMAND;
            } else {
                _debug_printf_P(PSTR("tokenizer('%s')\n"), command);
                argc = (int8_t)tokenizer(command, args, AT_MODE_MAX_ARGUMENTS, true);
            }

            _debug_printf_P(PSTR("Command '%s' argc %d arguments '%s'\n"), command, argc, implode(F("','"), (const char **)args, argc).c_str());

            // if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(FLASH))) {

            //     flash();

            // } else

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
                if (argc == 1 && *args[0] == 's') {
                    resetDetector.setSafeMode(1);
                    output.println(F("Software reset, safe mode enabled..."));
                }
                else {
                    output.println(F("Software reset..."));
                }
                config.restartDevice();
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(CMDS))) {
                output.print(F("+CMDS="));
                at_mode_print_command_string(output, ',');
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
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(IMPORT))) {
                if (argc >= 1) {
                    bool res = false;
                    auto file = SPIFFS.open(args[0], "r");
                    if (file) {
                        output.printf_P(PSTR("+IMPORT: %s"), args[0]);
                        Configuration::Handle_t *handlesPtr = nullptr;
                        Configuration::Handle_t handles[AT_MODE_MAX_ARGUMENTS + 1];
                        if (argc >= 2) {;
                            output.print(F(": "));
                            handlesPtr = &handles[0];
                            for(uint8_t i = 1; i < argc; i++) {
                                handles[i - 1] = (uint16_t)strtoul(args[i], nullptr, 0);
                                handles[i] = 0;
                                output.printf_P(PSTR("0x%04x "), handles[i - 1]);
                            }
                        }
                        output.println();
                        res = config.importJson(file, handlesPtr);
                    }
                    if (res) {
                        at_mode_print_ok(output);
                        config.write();
                        config.setConfigDirty(true);
                    } else {
                        output.printf_P(PSTR("+IMPORT: Failed to import: %s\n"), args[0]);
                    }
                }
                else {
                    at_mode_print_invalid_arguments(output);
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(FACTORY))) {
                config.restoreFactorySettings();
                at_mode_print_ok(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(ATMODE))) {
                if (argc != 1) {
                    at_mode_print_invalid_arguments(output);
                } else {
                    if (atoi(args[0])) {
                        enable_at_mode(output);
                    } else {
                        disable_at_mode(output);
                    }
                }
            }
            else if (!strcasecmp_P(command, PSTR("I2CT")) || !strcasecmp_P(command, PSTR("I2CR"))) {
                // ignore SerialTwoWire communication
            }
#if 0
            else if (!strcasecmp_P(command, PSTR("DIMTEST"))) {
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
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(WIFI))) {
                if (argc == 1) {
                    config.reconfigureWiFi();
                }

                auto flags = config._H_W_GET(Config().flags);
                output.printf_P("+WIFI: DHCP %u, station mode %s, SSID %s, connected %u, IP %s\n",
                    flags.softAPDHCPDEnabled,
                    (flags.wifiMode & WIFI_STA) ? PSTR("on") : PSTR("off"),
                    config._H_STR(Config().wifi_ssid),
                    WiFi.isConnected(),
                    WiFi.localIP().toString().c_str()
                );
            }
#if RTC_SUPPORT
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RTC))) {
                if (argc != 1) {
                    output.printf_P(PSTR("+RTC: time=%u, rtc=%u\n"), (uint32_t)time(nullptr), config.getRTC());
                    output.print(F("+RTC: "));
                    config.printRTCStatus(output);
                    output.println();
                }
                else {
                    output.printf_P(PSTR("+RTC: set=%u, rtc=%u\n"), config.setRTC(time(nullptr)), config.getRTC());
                }
            }
#endif
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(REM))) {
                // ignore comment
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DLY))) {
                unsigned long ms = 1;
                if (argc > 0) {
                    ms = atol(args[0]);
                }
                output.printf_P(PSTR("+DLY: %lu\n"), ms);
                delay(ms);
                at_mode_print_ok(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(DEL))) {
                if (argc != 1) {
                    at_mode_print_invalid_arguments(output);
                }
                else {
                    String filename = args[0];
                    auto result = SPIFFS.remove(filename);
                    output.printf_P(PSTR("+DEL: %s: %s\n"), filename.c_str(), result ? PSTR("success") : PSTR("failure"));
                }
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(CAT))) {
                if (argc != 1) {
                    at_mode_print_invalid_arguments(output);
                }
                else {
                    File file = SPIFFS.open(args[0], "r");
                    if (file) {
                        Scheduler.addTimer(10, true, [&output, file](EventScheduler::TimerPtr timer) mutable {
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
                                output.println();
                                output.printf_P(PSTR("+CAT: %s: %u\n"), file.name(), (unsigned)file.size());
                                file.close();
                                timer->detach();
                            }
                        });
                    }
                    else {
                        output.printf_P(PSTR("+CAT: Failed to open: %s\n"), args[0]);
                    }
                }
            }
#if DEBUG_HAVE_SAVECRASH
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHC))) {
                SaveCrash.clear();
                output.println(F("+SAVECRASH: cleared"));
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHP))) {
                SaveCrash.print(output);
            }
#endif
#if DEBUG
    #if PIN_MONITOR
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(PINM))) {
                if (!PinMonitor::getInstance()) {
                    output.println(F("+PINM: Pin monitor not initialized"));
                }
                else if (argc == 1) {
                    int state = atoi(args[0]);
                    auto &timer = PinMonitor::getTimer();
                    if (state && !timer) {
                        output.println(F("+PINM: started"));
                        Stream *serialPtr = &output;
                        Scheduler.addTimer(&timer, 1000, true, [serialPtr](EventScheduler::TimerPtr timer) {
                            PinMonitor::getInstance()->dumpPins(*serialPtr);
                        });
                    }
                    else if (!state && timer) {
                        Scheduler.removeTimer(timer);
                        timer = nullptr;
                        output.println(F("+PINM: stopped"));
                    }
                } else {
                    PinMonitor::getInstance()->dumpPins(output);
                }
            }
    #endif
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(PLUGINS))) {
                dump_plugin_list(output);
            }
            else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || !strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || !strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                if (argc == 1) {
                    float interval = atof(args[0]);
                    DisplayTypeEnum_t type;
                    PGM_P cmd;
                    if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(RSSI))) {
                        type = RSSI;
                        cmd = PSTR("RSSI");
                    }
                    else if (!strcasecmp_P(command, PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                        type = GPIO;
                        cmd = PSTR("GPIO");
                    }
                    else {
                        type = HEAP;
                        cmd = PSTR("HEAP");
                    }
                    rssiMin = -10000;
                    rssiMax = 0;
                    if (interval == 0) {
                        Scheduler.removeTimer(heapTimer);
                        heapTimer = nullptr;
                        output.printf_P(PSTR("+%s: Interval disabled"), cmd);
                    } else {
                        output.printf_P(PSTR("+%s: Interval set to %d seconds\n"), cmd);
                        create_heap_timer(interval, type);
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
                delay(100);
                panic();
            }
#endif
#if DEBUG_COLLECT_STRING_ENABLE
            else if (!strcasecmp_P(command, PSTR("JSONSTRDUMP"))) {
                __debug_json_string_dump(output);
            }
#endif
            else {
                bool commandWasHandled = false;
                for(auto plugin : plugins) { // send command to plugins
                    if (plugin->hasAtMode()) {
                        if (true == (commandWasHandled = plugin->atModeHandler(output, command, argc, args))) {
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
}


void at_mode_serial_input_handler(uint8_t type, const uint8_t *buffer, size_t len) {
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

#endif
