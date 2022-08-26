/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "ws_console_client.h"
#include <StreamString.h>
#include "http2serial.h"
#include "at_mode.h"
#include "reset_detector.h"

#if DEBUG_WS_CONSOLE_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void WsConsoleClient::onAuthenticated(uint8_t *data, size_t len)
{
    __LDBG_printf("data=%s len=%d", printable_string(data, len, 32).c_str(), len);
    #if AT_MODE_SUPPORTED
        StreamString commands;
        commands.print(F("+ATMODE_CMDS_HTTP2SERIAL="));
        #if AT_MODE_HELP_SUPPORTED
            at_mode_print_command_string(commands, '\t');
        #endif
        getClient()->text(commands);
    #endif
    if (resetDetector.hasCrashDetected()) {
        getClient()->text(PrintString(F("+REM System crash detected: %s\n"), resetDetector.getResetInfo().c_str()));
    }

    getClient()->text(PrintString(F("+CLIENT_ID=%p\r\n"), getClient()));
}

void WsConsoleClient::onText(uint8_t *data, size_t len)
{
    auto http2serial = Http2Serial::getInstance();
    if (http2serial) {
        String tmp;
        if (len > 10) {
            auto str = reinterpret_cast<const char *>(data);
            if (*str == '+' || strncmp_P(str, PSTR("AT+"), 3) == 0) {
                tmp.concat(str, len);
                auto pos1 = tmp.indexOf('=');
                if (pos1 != -1) {
                    auto pos2 = tmp.indexOf(F("$clientid"));
                    if (pos2 > pos1) {
                        tmp.replace(F("$clientid"), PrintString(F("%p"), this->getClient()));
                        data = reinterpret_cast<uint8_t *>(tmp.begin());
                        len = tmp.length();
                    }
                }
            }
        }
        WsClient::broadcast(nullptr, this, reinterpret_cast<const char *>(data), len);
        http2serial->write(data, len);
    }
}
