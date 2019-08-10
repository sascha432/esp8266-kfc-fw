/**
 * Author: sascha_lammers@gmx.de
 */

// web socket for dimmer.html

#include "dimmer_web_socket.h"
#include  <HttpHeaders.h>
#include "dimmer_module.h"
#include "progmem_data.h"
#include "web_socket.h"

#ifdef DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(dimmer_socket_uri, "/dimmer_ws");

AsyncWebSocket *wsDimmer = nullptr;
WsDimmerClient *WsDimmerClient::_sender = nullptr;

void dimmer_module_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsDimmerClient::onWsEvent(server, client, (int)type, data, len, arg, WsDimmerClient::getInstance);
}

void WsDimmerClient::setup() {
    if (get_web_server_object()) {
        wsDimmer = _debug_new AsyncWebSocket(FSPGM(dimmer_socket_uri));
        wsDimmer->onEvent(dimmer_module_event_handler);
        web_server_add_handler(wsDimmer);
        _debug_printf_P(PSTR("Web socket for dimmer module running on port %u\n"), config._H_GET(Config().http_port));
    }
}

void WsDimmerClient::broadcast(const String &message) {
    _debug_printf_P(PSTR("WsDimmerClient::broadcast(%s)\n"), message.c_str());

    if (WsClientManager::getWsClientCount(true)) {
        for(const auto &pair: WsClientManager::getWsClientManager()->getClients()) {
            if ( pair.wsClient != _sender && pair.socket->server() == wsDimmer && pair.socket->status() == WS_CONNECTED && pair.wsClient->isAuthenticated()) {
                pair.socket->text(message.c_str(), message.length());
            }
        }
    }    
}

WsClient *WsDimmerClient::getInstance(AsyncWebSocketClient *socket) {

    _debug_println(F("WsDimmerClient::getInstance()"));
    WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
    if (!wsClient) {
        wsClient = _debug_new WsDimmerClient(socket);
    }
    return wsClient;
}

void WsDimmerClient::onText(uint8_t *data, size_t len) {

    _debug_printf_P(PSTR("WsDimmerClient::onText(%p, %d)\n"), data, len);
    auto client = getClient();
    auto dimmer = Driver_DimmerModule::getInstance();
    if (dimmer && isAuthenticated()) {
        Buffer buffer;

        if (len > 5 && strncmp_P(reinterpret_cast<char *>(data), PSTR("+SET "), 5) == 0 && buffer.reserve(len + 1 - 5)) {
            auto buf = buffer.getChar();
            len -= 5;
            strncpy(buf, reinterpret_cast<char *>(data + 5), len)[len] = 0;

            const char *delimiters = " ";
            char *channel_str = strtok(buf, delimiters);
            char *level_str = strtok(nullptr, delimiters);

            if (channel_str && level_str) {
                int channel = atoi(channel_str);
                int level = atoi(level_str);

                _sender = this;
                dimmer->setChannel(channel, level);
                _sender = nullptr;
            }
        }
    }
}

WsDimmerClient::WsDimmerClient(AsyncWebSocketClient *socket) : WsClient(socket) {
}


String DimmerTemplate::process(const String &key) {
    if (key == F("MAX_BRIGHTNESS")) {
        return String(IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
    }
    else if (key.startsWith(F("CHANNEL")) && key.endsWith(F("_VALUE"))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (!dimmer) {
            return _sharedEmptyString;
        }
        auto channel = (key.charAt(7) - '0');
        _debug_printf_P(PSTR("channel %u level %d\n"), channel, dimmer->getChannel(channel));
        if (channel >= IOT_DIMMER_MODULE_CHANNELS) {
            return _sharedEmptyString;
        }
        return String(dimmer->getChannel(channel));
    }
    return WebTemplate::process(key);
}
