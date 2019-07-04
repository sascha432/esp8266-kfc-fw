/**
 * Author: sascha_lammers@gmx.de
 */

#if HTTP2SERIAL

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "web_socket.h"

class WsConsoleClient : public WsClient {
public:
    WsConsoleClient(AsyncWebSocketClient *socket) : WsClient(socket) {
    }

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onAuthenticated(uint8_t *data, size_t len) override;
#if DEBUG
    virtual void onDisconnect(uint8_t *data, size_t len) override;
    virtual void onError(WsErrorType type, uint8_t *data, size_t len) override;
#endif
    virtual void onText(uint8_t *data, size_t len);
    virtual void onStart() override;
    virtual void onEnd() override;
};

#endif
