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

WsClient *WsConsoleClient::getInstance(AsyncWebSocketClient *socket)
{
    return new WsConsoleClient(socket);
}

void WsConsoleClient::onAuthenticated(uint8_t *data, size_t len)
{
    __LDBG_printf("data=%s len=%d", printable_string(data, len, 32).c_str(), len);
#if AT_MODE_SUPPORTED
    StreamString commands;
    commands.print(F("+ATMODE_CMDS_HTTP2SERIAL="));
    at_mode_print_command_string(commands, '\t');
    getClient()->text(commands);
#endif
    if (resetDetector.hasCrashDetected()) {
        getClient()->text(PrintString(F("+REM System crash detected: %s\n"), resetDetector.getResetInfo().c_str()));
    }
    getClient()->text(PrintString(F("+CLIENT_ID=%p\n"), getClient()));
}

void WsConsoleClient::onText(uint8_t *data, size_t len)
{
    auto http2serial = Http2Serial::getInstance();
    if (http2serial) {
        WsClient::broadcast(nullptr, this, reinterpret_cast<const char *>(data), len);
        http2serial->write(data, len);
    }
}

void WsConsoleClient::onStart()
{
    __LDBG_printf("first client has been authenticated, Http2Serial instance %p", Http2Serial::getInstance());
    Http2Serial::createInstance();
}

void WsConsoleClient::onEnd()
{
    __LDBG_printf("no authenticated clients connected, Http2Serial instance %p", Http2Serial::getInstance());
    Http2Serial::destroyInstance();
}
