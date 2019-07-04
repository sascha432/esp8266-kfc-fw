/**
 * Author: sascha_lammers@gmx.de
 */

#if HTTP2SERIAL

#include "ws_console_client.h"
#include "http2serial.h"
#include "at_mode.h"

WsClient *WsConsoleClient::getInstance(AsyncWebSocketClient *socket) {

    WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
    if (!wsClient) {
        wsClient = new WsConsoleClient(socket);
    }
    return wsClient;
}

void WsConsoleClient::onAuthenticated(uint8_t *data, size_t len) {
    if_debug_printf_P(PSTR("WsConsoleClient::onAuthenticated(%s, %d)\n"), printable_string(data, std::min((size_t)32, len)).c_str(), len);
#if AT_MODE_SUPPORTED
    getClient()->text(get_atmode_commands());
#endif
}

#if DEBUG

void WsConsoleClient::onDisconnect(uint8_t *data, size_t len) {
    if_debug_printf_P(PSTR("WsConsoleClient::onDisconnect(%s, %d)\n"), printable_string(data, std::min((size_t)32, len)).c_str(), len);
}

void WsConsoleClient::onError(WsErrorType type, uint8_t *data, size_t len) {
    if_debug_printf_P(PSTR("WsConsoleClient::onError(%d, %s, %d)\n"), type, printable_string(data, std::min((size_t)32, len)).c_str(), len);
}

#endif

void WsConsoleClient::onText(uint8_t *data, size_t len) {
    if (Http2Serial::_instance) {
        Http2Serial::_instance->broadcast(this, data, len); // send received text to all other clients
        // http2Serial->broadcast(data, len); // send received text to all clients = echo mode
        Http2Serial::_instance->getSerialHandler()->receivedFromRemote(nullptr, data, len); // send to serial
    }
}


void WsConsoleClient::onStart() {
    if_debug_printf_P(PSTR("WsConsoleClient::onStart() - first client has been authenticated, Http2Serial instance %p\n"), Http2Serial::_instance);
    if (!Http2Serial::_instance) {
        Http2Serial::_instance = new Http2Serial();
    }

}

void WsConsoleClient::onEnd() {
    if_debug_printf_P(PSTR("WsConsoleClient::onEnd() - no authenticated clients connected, Http2Serial instance %p\n"), Http2Serial::_instance);
    if (Http2Serial::_instance) {
        delete Http2Serial::_instance;
        Http2Serial::_instance = nullptr;
    }
}

#endif
