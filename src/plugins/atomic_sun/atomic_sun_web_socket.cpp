/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2

// web socket for dimmer.html

#include "atomic_sun_web_socket.h"
#include  <HttpHeaders.h>
#include "progmem_data.h"
#include "web_socket.h"
#include "atomic_sun_v2.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(atomic_sun_web_socket_uri, "/atomic_sun_ws");

AsyncWebSocket *wsAtomicSun = nullptr;
WsAtomicSunClient *WsAtomicSunClient::_sender = nullptr;

void atomic_sun_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsAtomicSunClient::onWsEvent(server, client, (int)type, data, len, arg, WsAtomicSunClient::getInstance);
}

void WsAtomicSunClient::setup() {
    if (get_web_server_object()) {
        wsAtomicSun = _debug_new AsyncWebSocket(FSPGM(atomic_sun_web_socket_uri));
        wsAtomicSun->onEvent(atomic_sun_event_handler);
        web_server_add_handler(wsAtomicSun);
        _debug_printf_P(PSTR("Web socket for Atomic Sun running on port %u\n"), config._H_GET(Config().http_port));
    }
}

void WsAtomicSunClient::broadcast(const String &message) {
    _debug_printf_P(PSTR("WsAtomicSunClient::broadcast(%s)\n"), message.c_str());

    if (WsClientManager::getWsClientCount(true)) {
        for(const auto &pair: WsClientManager::getWsClientManager()->getClients()) {
            if (pair.wsClient != _sender && pair.socket->server() == wsAtomicSun && pair.socket->status() == WS_CONNECTED && pair.wsClient->isAuthenticated()) {
                pair.socket->text(message.c_str(), message.length());
            }
        }
    }    
}

WsClient *WsAtomicSunClient::getInstance(AsyncWebSocketClient *socket) {

    _debug_println(F("WsAtomicSunClient::getInstance()"));
    WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
    if (!wsClient) {
        wsClient = _debug_new WsAtomicSunClient(socket);
    }
    return wsClient;
}

void WsAtomicSunClient::onText(uint8_t *data, size_t len) {

    _debug_printf_P(PSTR("WsAtomicSunClient::onText(%p, %d)\n"), data, len);
    auto dimmer = Driver_4ChDimmer::getInstance();
    if (dimmer && isAuthenticated()) {
        PrintString command;
        command.write(data, len);
        command.toLowerCase();

        if (command.startsWith(F("+color "))) {

            _sender = this;
            dimmer->setColor(command.substring(7).toInt());
            dimmer->setLevel(-1);
            dimmer->publishState();
            _sender = nullptr;
        }
        else if (command.startsWith(F("+setbc "))) {

            const char *delimiters = " ";
            char *buf = command.begin() + 7;
            char *brightness_str = strtok(buf, delimiters);
            char *color_str = strtok(nullptr, delimiters);
            if (brightness_str && color_str) {
                _sender = this;
                dimmer->setBrightness(atoi(brightness_str));
                dimmer->setColor(atoi(color_str));
                dimmer->setLevel(-1);
                dimmer->publishState();
                _sender = nullptr;
            }
        }
        else if (command.startsWith(F("+set "))) {

            _sender = this;
            dimmer->setBrightness(command.substring(5).toInt());
            dimmer->setLevel(-1);
            dimmer->publishState();
            _sender = nullptr;
        }
    }
}

WsAtomicSunClient::WsAtomicSunClient(AsyncWebSocketClient *socket) : WsClient(socket) {
}


String AtomicSunTemplate::process(const String &key) {
    if (key == F("MAX_BRIGHTNESS")) {
        return String(IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
    }
    else if (key == F("BRIGHTNESS")) {
        auto dimmer = Driver_4ChDimmer::getInstance();
        if (!dimmer) {
            return _sharedEmptyString;
        }
        return String(dimmer->getBrightness());
    }
    else if (key == F("COLOR")) {
        auto dimmer = Driver_4ChDimmer::getInstance();
        if (!dimmer) {
            return _sharedEmptyString;
        }
        return String(dimmer->getColor());
    }
    return WebTemplate::process(key);
}

#endif
