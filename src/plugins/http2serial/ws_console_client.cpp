/**
 * Author: sascha_lammers@gmx.de
 */

#if HTTP2SERIAL

#include "ws_console_client.h"
#include <StreamString.h>
#include "http2serial.h"
#include "at_mode.h"
#include "reset_detector.h"

#if DEBUG_HTTP2SERIAL
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
    _debug_printf_P(PSTR("data=%s len=%d\n"), printable_string(data, len, 32).c_str(), len);
#if AT_MODE_SUPPORTED
    StreamString commands;
    commands.print(F("+ATMODE_CMDS_HTTP2SERIAL="));
    at_mode_print_command_string(commands, '\t');
    getClient()->text(commands);
#endif
    if (resetDetector.hasCrashDetected()) {
        PrintString message;
        message.printf_P(PSTR("+REM System crash detected: %s\n"), resetDetector.getResetInfo().c_str());
        getClient()->text(message);
    }
    getClient()->text(PrintString(F("+CLIENT_ID=%p\n"), getClient()));
}

#if DEBUG

void WsConsoleClient::onDisconnect(uint8_t *data, size_t len)
{
    _debug_printf_P(PSTR("data=%s len=%d\n"), printable_string(data, len, 32).c_str(), len);
}

void WsConsoleClient::onError(WsConsoleClient::WsErrorType type, uint8_t *data, size_t len)
{
    _debug_printf_P(PSTR("type=%d data=%s len=%d\n"), type, printable_string(data, len, 32).c_str(), len);
}

#endif

void WsConsoleClient::onText(uint8_t *data, size_t len)
{
    auto http2serial = Http2Serial::getInstance();
    if (http2serial) {
        WsClient::broadcast(nullptr, this, reinterpret_cast<const char *>(data), len);
        // http2serial->broadcast(this, data, len); // send received text to all other clients
        // http2serial->broadcast(data, len); // send received text to all clients = echo mode
        http2serial->getSerialHandler()->receivedFromRemote(nullptr, data, len); // send to serial
    }
}


void WsConsoleClient::onStart()
{
    _debug_printf_P(PSTR("first client has been authenticated, Http2Serial instance %p\n"), Http2Serial::getInstance());
    Http2Serial::createInstance();
}

void WsConsoleClient::onEnd()
{
    _debug_printf_P(PSTR("no authenticated clients connected, Http2Serial instance %p\n"), Http2Serial::getInstance());
    Http2Serial::destroyInstance();
}

#endif
