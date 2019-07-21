/**
  Author: sascha_lammers@gmx.de
*/

#if PING_MONITOR

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "AsyncPing.h"
#include "web_socket.h"

#ifndef DEBUG_PING_MONITOR
#define DEBUG_PING_MONITOR 1
#endif

class WsPingClient : public WsClient {
public:
    WsPingClient( AsyncWebSocketClient *socket);
    virtual ~WsPingClient();

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;
    virtual void onDisconnect(uint8_t *data, size_t len) override;
    virtual void onError(WsErrorType type, uint8_t *data, size_t len) override;

    AsyncPing &getPing();

private:
    void _cancelPing();
    String _getHost(uint8_t num) const;
    AsyncPing _ping;
};

#endif
