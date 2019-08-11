/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2

#pragma once

#include "web_server.h"
#include "templates.h"
#include "web_socket.h"

class WsAtomicSunClient : public WsClient {
public:
    WsAtomicSunClient( AsyncWebSocketClient *socket);
    virtual ~WsAtomicSunClient() {
    }

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;

    static void broadcast(const String &message);
    static void setup();

private:
    static WsAtomicSunClient *_sender;
};


class AtomicSunTemplate : public WebTemplate {
public:
    virtual String process(const String &key) override;
};

#endif
