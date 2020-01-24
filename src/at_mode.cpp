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
    at_mode_help.emplace_back(std::move(ATModeCommandHelp(help, pluginName)));
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
    char lastChar = 0;
    if (output.length()) {
        lastChar = output.charAt(output.length() - 1);
    }

    if (str) {
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
    _debug_printf_P(PSTR("at_mode_display_help(): size=%d, find=%s\n"), at_mode_help.size(), findText ? (findText->empty() ? PSTR("count=0") : implode(FSPGM(comma), findText).c_str()) : PSTR("nullptr"));
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
            _appendHelpString(tmp, commandHelp.command);
            _appendHelpString(tmp, commandHelp.arguments);
            _appendHelpString(tmp, commandHelp.help);
            _appendHelpString(tmp, commandHelp.helpQueryMode);

            _debug_printf_P(PSTR("at_mode_display_help(): find in %u: '%s'\n"), tmp.length(), tmp.c_str());
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
            if (commandHelp.command) {
                output.printf_P(PSTR(" AT+%s?\n"), commandHelp.command);
            } else {
                output.print(F(" AT?\n"));
            }
            at_mode_display_help_indent(output, commandHelp.helpQueryMode);
        }

        if (commandHelp.command) {
            output.printf_P(PSTR(" AT+%s"), commandHelp.command);
        } else {
            output.print(F(" AT"));
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
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LS, "LS", "[<directory>]", "List files and directory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WIFI, "WIFI", "[<reconnect>]", "Display WiFi info");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RADC, "RADC", "<number=3>,<delay=5ms>", "Turn WiFi off and read ADC");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(REM, "REM", "Ignore comment");
#if RTC_SUPPORT
PROGMEM_AT_MODE_HELP_COMMAND_DEF(RTC, "RTC", "[<set>]", "Set RTC time", "Display RTC time");
#endif

#if DEBUG_HAVE_SAVECRASH
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHC, "SAVECRASHC", "Clear crash memory");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SAVECRASHP, "SAVECRASHP", "Print saved crash details");
#endif

#if DEBUG

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(FSM, "FSM", "Display FS mapping");
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

void at_mode_help_commands()
{
    auto name = PSTR("at_mode");
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(AT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DSLP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RST), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CMDS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOAD), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(IMPORT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(STORE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FACTORY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(ATMODE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DLY), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CAT), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DEL), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIFI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RADC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(REM), name);
#if RTC_SUPPORT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTC), name);
#endif

#if DEBUG_HAVE_SAVECRASH
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SAVECRASHP), name);
#endif

#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(FSM), name);
#if PIN_MONITOR
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PINM), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PLUGINS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HEAP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RSSI), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(GPIO), name);
#if defined(ESP8266)
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(CPU), name);
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPFS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPEE), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WRTC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DUMPRTC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCCLR), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RTCQCC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(WIMO), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(LOG), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PANIC), name);
#endif

}

void at_mode_generate_help(Stream &output, StringVector *findText = nullptr)
{
    _debug_printf_P(PSTR("at_mode_generate_help(): find=%s\n"), implode(FSPGM(comma), findText).c_str());
    // call handler to gather help for all commands
    at_mode_help_commands();
    for(auto plugin: plugins) {
        plugin->atModeHelpGenerator();
    }
    at_mode_display_help(output, findText);
    at_mode_help.clear();
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

    DisplayTimer() : _timer(nullptr), _type(HEAP) {
    }

    void removeTimer() {
        if (Scheduler.hasTimer(_timer)) {
            Scheduler.removeTimer(_timer);
        }
        _timer = nullptr;
    }

public:
    EventScheduler::TimerPtr _timer = nullptr;
    DisplayTypeEnum_t _type = HEAP;
    int32_t _rssiMin, _rssiMax;
};

DisplayTimer displayTimer;

static void heap_timer_callback(EventScheduler::TimerPtr timer)
{
    if (displayTimer._type == DisplayTimer::HEAP) {
        MySerial.printf_P(PSTR("+HEAP: Free %u CPU %d MHz\n"), ESP.getFreeHeap(), ESP.getCpuFreqMHz());
    }
    else if (displayTimer._type == DisplayTimer::GPIO) {
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
#warning not implemented
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
    if (Scheduler.hasTimer(displayTimer._timer)) {
        displayTimer._timer->changeOptions(seconds * 1000);
    } else {
        Scheduler.addTimer(&displayTimer._timer, seconds * 1000, true, heap_timer_callback, EventScheduler::PRIO_LOW);
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

void at_mode_setup()
{
    SerialHandler::getInstance().addHandler(at_mode_serial_input_handler, SerialHandler::RECEIVE|SerialHandler::REMOTE_RX);
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::DISCONNECTED, at_mode_wifi_callback);
}

void enable_at_mode(Stream &output)
{
    auto &flags = config._H_W_GET(Config().flags);
    if (!flags.atModeEnabled) {
        output.println(F("Enabling AT MODE."));
        flags.atModeEnabled = true;
    }
}

void disable_at_mode(Stream &output)
{
    auto &flags = config._H_W_GET(Config().flags);
    if (flags.atModeEnabled) {
        output.println(F("Disabling AT MODE."));
#if DEBUG
        displayTimer.removeTimer();
#endif
        flags.atModeEnabled = false;
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

void at_mode_print_invalid_command(Stream &output)
{
    output.println(F("ERROR - Invalid command. AT? for help"));
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

void at_mode_serial_handle_event(String &commandString)
{
    auto &output = MySerial;
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
                tokenizer(command, args, true);
            }

            _debug_printf_P(PSTR("Command '%s' argc %d arguments '%s'\n"), command, args.size(), implode(F("','"), &args.getArgs()).c_str());

            args.setCommand(command);

            if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DSLP))) {
                if (args.isQueryMode()) {
                    args.printf_P(PSTR("Max. deep sleep %.3f seconds"), ESP.deepSleepMax() / 1000000.0);
                }
                else {
                    uint32_t time = args.toMillis(0);
                    RFMode mode = (RFMode)args.toInt(1, RF_DEFAULT);
                    args.print(F("Entering deep sleep..."));
                    config.enterDeepSleep(time, mode, 1);
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLP))) {
                String plugin;
                StringVector findItems;
                for(auto strPtr: args.getArgs()) {
                    String str = strPtr;
                    str.trim();
                    str.toLowerCase();
                    findItems.emplace_back(std::move(str));
                }
                at_mode_generate_help(output, &findItems);
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RST))) {
                if (args.equals(0, 's')) {
                    resetDetector.setSafeMode(1);
                    args.print(F("Software reset, safe mode enabled..."));
                }
                else {
                    args.print(F("Software reset..."));
                }
                config.restartDevice();
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RADC))) {
                int numReads = args.toInt(0, 3);
                int readDelay = args.toInt(1, 5);
                WiFi.mode(WIFI_OFF);
                system_soft_wdt_stop();
                ets_intr_lock( );
                noInterrupts();
                uint32_t total = 0;
                for(int i = 0; i < numReads; i++) {
                    total += system_adc_read();
                    delay(readDelay);
                }
                interrupts();
                ets_intr_unlock();
                system_soft_wdt_restart();
                config.reconfigureWiFi();
                args.printf_P(PSTR("Total=%u,num=%u,avg=%.2f"), total, numReads, total / (float)numReads);

                Scheduler.addTimer(10000, false, [=](EventScheduler::TimerPtr) {
                    Logger_notice(F("ADC total=%u,num=%u,avg=%.2f"), total, numReads, total / (float)numReads);
                });

            }
            else if (args.isCommand(PSTR("I2CT")) || args.isCommand(PSTR("I2CR"))) {
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
                    config.reconfigureWiFi();
                }

                auto flags = config._H_W_GET(Config().flags);
                args.printf_P("DHCP %u, station mode %s, SSID %s, connected %u, IP %s",
                    flags.softAPDHCPDEnabled,
                    (flags.wifiMode & WIFI_STA) ? PSTR("on") : PSTR("off"),
                    config._H_STR(Config().wifi_ssid),
                    WiFi.isConnected(),
                    WiFi.localIP().toString().c_str()
                );
            }
#if RTC_SUPPORT
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RTC))) {
                if (!args.empty()) {
                    args.printf_P(PSTR("Time=%u, rtc=%u"), (uint32_t)time(nullptr), config.getRTC());
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DEL))) {
                if (args.requireArgs(1, 1)) {
                    auto filename = args.get(0);
                    auto result = SPIFFS.remove(filename);
                    args.printf_P(PSTR("%s: %s"), filename, result ? PSTR("success") : PSTR("failure"));
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LS))) {
                Dir dir = SPIFFSWrapper::openDir(args.get(0));
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(CAT))) {
                if (args.requireArgs(1, 1)) {
                    auto filename = args.get(0);
                    auto file = SPIFFSWrapper::open(filename, fs::FileOpenMode::read);
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
                                output.printf_P(PSTR("\n+%s: %s: %u\n"), PROGMEM_AT_MODE_HELP_COMMAND(CAT), file.name(), (unsigned)file.size());
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
#if DEBUG_HAVE_SAVECRASH
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHC))) {
                SaveCrash.clear();
                args.print(F("Cleared"));
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SAVECRASHP))) {
                SaveCrash.print(output);
            }
#endif
#if DEBUG
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(FSM))) {
                Mappings::getInstance().dump(output);
            }
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RSSI)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HEAP)) || args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(GPIO))) {
                if (args.requireArgs(1, 1)) {
                    auto interval = args.toMillis(0, 500);
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
                config.dump(output);
            }
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
                    uint32_t data = (uint32_t)strtoul(args.get(1), nullptr, 0); // auto detect base
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
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WIMO))) {
                if (args.requireArgs(1, 1)) {
                    args.print(F("Setting WiFi mode and restarting device..."));
                    config._H_W_GET(Config().flags).wifiMode = args.toInt(0);
                    config.write();
                    config.restartDevice();
                }
            }
            else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LOG))) {
                if (args.requireArgs(1)) {
                    Logger_error(F("+LOG: %s"), implode(FSPGM(comma), &args.getArgs()).c_str());
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
                for(auto plugin : plugins) { // send command to plugins
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

#endif
