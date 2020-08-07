/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "web_socket.h"

#ifndef DEBUG_WS_CONSOLE_CLIENT
#define DEBUG_WS_CONSOLE_CLIENT                 0
#endif

class WsConsoleClient : public WsClient {
public:
    using WsClient::WsClient;

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onAuthenticated(uint8_t *data, size_t len) override;
    virtual void onText(uint8_t *data, size_t len);
    virtual void onStart() override;
    virtual void onEnd() override;


#if DEBUG_WS_CONSOLE_CLIENT
    virtual void onDisconnect(uint8_t *data, size_t len) override {
        __DBG_printf("data=%s len=%d", printable_string(data, len, 32).c_str(), len);
    }
    virtual void onError(WsErrorType type, uint8_t *data, size_t len) override {
        __DBG_printf("type=%d data=%s len=%d", type, printable_string(data, len, 32).c_str(), len);
    }
#endif
};
