/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "at_mode_commands.h"
#include "kfc_fw_config.h"
#include "misc.h"
#include "serial_handler.h"
#include "plugins.h"

#if ESP8266
#    include <core_version.h>
#    if ARDUINO_ESP8266_MAJOR == 0
#        error Invalid core config
#   endif
#endif

#if DEBUG_AT_MODE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

ATMode atMode;

void ATMode::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        Serial.printf_P(PSTR("WiFi connected to %s - IP "), WiFi.SSID().c_str());
        WiFi.localIP().printTo(Serial);
        Serial.println();
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        Serial.println(F("WiFi connection lost"));
    }
}

void ATMode::setup()
{
    enabled = KFCConfigurationClasses::System::Flags::getConfig().is_at_mode_enabled;
    if (client) {
        serialHandler.removeClient(*client);
    }
    client = &serialHandler.addClient([this](Stream &stream) {
        serialInputHandler(stream);
    }, SerialHandler::EventType::READ);

    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
}

void ATMode::disable(Stream *output)
{
    if (enabled) {
        if (output) {
            output->println(F("Disabling AT MODE."));
        }
        #if DEBUG
            if (atModePrintLoop) {
                atModePrintLoop->remove();
            }
        #endif
        lastWasCR = false;
        enabled = false;
    }
}

void ATMode::printHelp(Stream &output)
{
    output.println(F("try https://github.com/sascha432/esp8266-kfc-fw/blob/master/docs/AtModeHelp.md\n"));
    if (config.isSafeMode()) {
        output.println(F("SAFE MODE ENABLED"));
    }
}

void ATMode::printInvalidArguments(Stream &output, uint16_t num, uint16_t min, uint16_t max)
{
    output.print(F("ERROR - "));
    if (min != kUnset) {
        if (min == max || max == kUnset) {
            output.printf_P(PSTR("Expected %u argument(s), got %u\n"), min, num);
        }
        else {
            output.printf_P(PSTR("Expected %u to %u argument(s), got %u\n"), min, max, num);
        }
    }
    else {
        output.println(F("Invalid arguments"));
    }
    printHelp(output);
}

bool ATMode::tokenizerCmdLineMode(char ch, int type)
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

void ATMode::handleEvent(String &commandString)
{
    // process all chained commands (command1;command2;...) without recursion to keep
    // the stack usage constant, the buffer is modified in place
    for (;;) {
        char *nextCommand = nullptr;
        auto &output = Serial;
        AtModeArgs args(output);
        bool atModeCommands = true;

        // determine if query mode by checking for a trailing '?'
        commandString.trim();
        bool isQueryMode = StrView(commandString).endsWith('?');

        // check command prefix
        if (StrView(commandString).startsWithIgnoreCase(F("AT"))) {
            // remove AT from the command
            commandString.remove(0, 2);
        }
        else if (kAllowShortPrefix && StrView(commandString).startsWith('+')) {
            // allow using AT+COMMAND[?|=<args,...>] and +COMMAND[?|=<args,...>]
        }
        else if __CONSTEXPR17 (kAllowNoPrefix) {
            // allow using COMMAND [<arg1>][ <args...>] (or /COMMAND, --COMMAND -COMMAND)
            atModeCommands = false;
        }
        else {
            // display invalid command if it does not start with the allowed prefix
            printInvalidCommand(output);
            return;
        }

        // check for empty commands
        if (commandString.length() == 0) { // AT
            if (atModeCommands) { // display OK for empty AT commands and ignore empty lines
                args.ok();
            }
            return;
        }

        auto command = commandString.begin();
        // remove leading '+'
        if (*command == '+') {
            command++;
        }
        // removing leading '/', '-' and '--'
        else if (kAllowNoPrefix && !atModeCommands) {
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
            __LDBG_printf("cmd=%s,argc=%u,args='%s',next_cmd='%s'", command, args.size(), __S(implode(F("' '"), args.getArgs())), __S(nextCommand));
        }
        else if __CONSTEXPR17 (kAllowNoPrefix) {
            // run tokenizer in cli mode
            // command arg1 arg2 arg3 ; command2 ...
            args.setQueryMode(false);
            tokenizer(command, args, true, &nextCommand, tokenizerCmdLineMode);
            __LDBG_printf("cmd=%s,argc=%u,args='%s',next_cmd='%s'", __S(command), args.size(), __S(implode(F("' '"), args.getArgs())), __S(nextCommand));
        }
        // store copy of command
        args.setCommand(command);

        if (!ATModeCommands::handle(args)) {
            bool commandWasHandled = false;
            for(const auto plugin: PluginComponents::Register::getPlugins()) { // send command to plugins
                if (plugin->hasAtMode()) {
                    if (true == (commandWasHandled = plugin->atModeHandler(args))) {
                        break;
                    }
                }
            }
            if (!commandWasHandled) {
                printInvalidCommand(output);
            }
        }

        if (!nextCommand) {
            break;
        }
        // drop the processed command and continue with the next one
        auto offset = static_cast<unsigned int>(nextCommand - commandString.begin());
        if (offset == 0) {
            // no progress, stop to avoid an endless loop
            break;
        }
        commandString.remove(0, offset);
    }
}

void ATMode::serialInputHandler(Stream &stream)
{
    if (!enabled) {
        // drain the input, otherwise the serial handler keeps invoking this callback
        // as long as the client's rx buffer is not empty
        while (stream.available()) {
            stream.read();
        }
        return;
    }

    auto &line = serialHandler.inputBuffer;
    auto serial = StreamWrapper(serialHandler.getStreams(), serialHandler.getInput()); // local output only

    while(stream.available()) {
        int ch = stream.read();
        // __DBG_printf("read %u (%c) cr=%u", (unsigned)((uint8_t)ch), isprint(ch) ? ch : '-', lastWasCR);
        // swallow the LF of a CRLF pair and reset the flag on any other character
        const bool wasCR = lastWasCR;
        lastWasCR = (ch == '\r');
        if (wasCR && ch == '\n') {
            continue;
        }
        switch(ch) {
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
                serial.write('\n');
                handleEvent(line);
                line.clear();
                break;
            case '\r':
                serial.println();
                handleEvent(line);
                line.clear();
                break;
            default:
                if (ch >= 128) {
                    serial.printf_P(PSTR("Serial input - invalid character %u\r\n"), ch);
                    break;
                }
                line += (char)ch;
                serial.write(ch);
                if (line.length() >= kSerialInputBufferSize) {
                    serial.write('\n');
                    handleEvent(line);
                    line.clear();
                }
                break;
        }
    }
}
