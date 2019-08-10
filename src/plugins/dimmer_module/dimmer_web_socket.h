/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "web_server.h"
#include "templates.h"
#include "web_socket.h"

class WsDimmerClient : public WsClient {
public:
    WsDimmerClient( AsyncWebSocketClient *socket);
    virtual ~WsDimmerClient() {
    }

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;

    static void broadcast(const String &message);
    static void setup();

private:
    static WsDimmerClient *_sender;
};


class DimmerTemplate : public WebTemplate {
public:
    virtual String process(const String &key) override;
};
