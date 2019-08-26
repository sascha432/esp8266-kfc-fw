/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <JsonBuffer.h>
#include "plugins.h"
#include "session.h"
#include "WebUISocket.h"

#if IOT_DIMMER_MODULE
#include "plugins/dimmer_module/dimmer_module.h"
#endif

#if IOT_ATOMIC_SUN_V2
#include "plugins/atomic_sun/atomic_sun_v2.h"
#endif

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


PROGMEM_STRING_DEF(webui_socket_uri, "/webui_ws");

AsyncWebSocket *wsWebUI = nullptr;
WsWebUISocket *WsWebUISocket::_sender = nullptr;

void webui_socket_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsWebUISocket::onWsEvent(server, client, (int)type, data, len, arg, WsWebUISocket::getInstance);
}

void WsWebUISocket::setup() {
    wsWebUI = _debug_new AsyncWebSocket(FSPGM(webui_socket_uri));
    wsWebUI->onEvent(webui_socket_event_handler);
    web_server_add_handler(wsWebUI);
    _debug_printf_P(PSTR("Web socket for UI running on port %u\n"), config._H_GET(Config().http_port));
}

// void WsWebUISocket::send(AsyncWebSocketClient *client, const String &id, bool state, const String &value) {
//     JsonUnnamedObject json;
//     json.add(JJ(type), JJ(ue));
//     auto &events = json.addArray(JJ(events), 1);
//     auto &event = events.addObject(1);
//     event.add(JJ(id), id.c_str());
//     event.add(JJ(value), value.c_str());
//     event.add(JJ(state), state);
//     if (client) {
//         send(client, json);
//     } else {
//         broadcast(json);
//     }
// }

// void WsWebUISocket::send(AsyncWebSocketClient *client, const String &id, bool state) {
//     JsonUnnamedObject json;
//     json.add(JJ(type), JJ(ue));
//     auto &events = json.addArray(JJ(events), 1);
//     auto &event = events.addObject(1);
//     event.add(JJ(id), id.c_str());
//     event.add(JJ(state), state);
//     if (client) {
//         send(client, json);
//     } else {
//         broadcast(json);
//     }
// }

// void WsWebUISocket::send(AsyncWebSocketClient *client, const String &id, const String &value) {
//     JsonUnnamedObject json;
//     json.add(JJ(type), JJ(ue));
//     auto &events = json.addArray(JJ(events), 1);
//     auto &event = events.addObject(1);
//     event.add(JJ(id), id.c_str());
//     event.add(JJ(value), value.c_str());
//     if (client) {
//         send(client, json);
//     } else {
//         broadcast(json);

//     }
// }

void WsWebUISocket::send(AsyncWebSocketClient *client, JsonUnnamedObject &json) {
    auto buffer = wsWebUI->makeBuffer(json.length());
    assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
    client->text(buffer);
}


// void WsWebUISocket::_broadcast(const String &message) {
//     _debug_printf_P(PSTR("WsWebUISocket::broadcast(%s)\n"), message.c_str());

//     if (WsClientManager::getWsClientCount(true)) {
//         for(const auto &pair: WsClientManager::getWsClientManager()->getClients()) {
//             if (pair.wsClient != _sender && pair.socket->server() == wsWebUI && pair.socket->status() == WS_CONNECTED && pair.wsClient->isAuthenticated()) {
//                 pair.socket->text(message);
//             }
//         }
//     }
// }

void WsWebUISocket::_broadcast(AsyncWebSocketMessageBuffer *buffer) {
    _debug_printf_P(PSTR("WsWebUISocket::broadcast(%s)\n"), buffer->get());

    if (WsClientManager::getWsClientCount(true)) {
        buffer->lock();
        for(const auto &pair: WsClientManager::getWsClientManager()->getClients()) {
            if (pair.wsClient != _sender && pair.socket->server() == wsWebUI && pair.socket->status() == WS_CONNECTED && pair.wsClient->isAuthenticated()) {
                pair.socket->text(buffer);
            }
        }
        buffer->unlock();
        wsWebUI->_cleanBuffers();
    }
}

void WsWebUISocket::broadcast(JsonUnnamedObject &json) {
    auto buffer = wsWebUI->makeBuffer(json.length());
    assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
    _broadcast(buffer);
}


WsClient *WsWebUISocket::getInstance(AsyncWebSocketClient *socket) {

    _debug_println(F("WsWebUISocket::getInstance()"));
    WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
    if (!wsClient) {
        wsClient = _debug_new WsWebUISocket(socket);
    }
    return wsClient;
}


void WsWebUISocket::onText(uint8_t *data, size_t len) {

    _debug_printf_P(PSTR("WsWebUISocket::onText(%p, %d)\n"), data, len);
    if (isAuthenticated()) {
        auto client = getClient();
        String command;
        const uint8_t maxArgs = 4;
        String args[maxArgs];
        uint8_t argc;

        auto ptr = (char *)data;
        while(len && !isspace(*ptr)) {
            command += (char)tolower(*ptr++);
            len--;
        }

        argc = 0;
        while(len) {
            while(len && isspace(*ptr)) {
                ptr++;
                len--;
            }
            while(len && !isspace(*ptr)) {
                args[argc] += *ptr++;
                len--;
            }
            if (++argc == maxArgs) {
                break;
            }
        }

        _debug_printf_P(PSTR("WsWebUISocket::onText():command=%s,args=%s\n"), command.c_str(), implode(F(","), args, argc).c_str());

        if (strcasecmp_P(command.c_str(), PSTR("+get_values")) == 0) {
            sendValues(client);
        }
        if (strcasecmp_P(command.c_str(), PSTR("+set_state")) == 0) {
            bool state = args[1].toInt() || (strcasecmp_P(args[1].c_str(), PSTR("true")) == 0);
            for(auto plugin: plugins) {
                if (plugin->hasWebUI()) {
                    auto interface = plugin->getWebUIInterface();
                    if (interface) {
                        interface->setValue(args[0], String(), false, state, true);
                    }
                }
            }
        }
        else if (strcasecmp_P(command.c_str(), PSTR("+set")) == 0) {
            _sender = this;
            for(auto plugin: plugins) {
                if (plugin->hasWebUI()) {
                    auto interface = plugin->getWebUIInterface();
                    if (interface) {
                        interface->setValue(args[0], args[1], true, false, false);
                    }
                }
            }
            _sender = nullptr;
        }
    }
}

void WsWebUISocket::sendValues(AsyncWebSocketClient *client) {

    JsonUnnamedObject json(1);

    json.add(JJ(type), JJ(ue));
    auto &array = json.addArray(JJ(events));

    for(auto plugin: plugins) {
        if (plugin->hasWebUI()) {
            auto interface = plugin->getWebUIInterface();
            if (interface) {
                interface->getValues(array);
            }
        }
    }

    send(client, json);
}

void WsWebUISocket::createWebUIJSON(JsonUnnamedObject &json) {

    WebUI webUI(json);

    // auto row = &webUI.addRow();
    // row->setExtraClass(F("webuicomponent-top"));
    // row->setAlignment(WebUIRow::CENTER);

    // row->addBadgeSensor(F("temperature"), F("Temperature"), F("°C")).setValue(F("25.78"));
    // row->addBadgeSensor(F("humidity"), F("Humidity"), F("%")).setValue(F("47.23"));
    // row->addBadgeSensor(F("pressure"), F("Pressure"), F("hPa")).setValue(F("1023.42"));
    // row->addBadgeSensor(F("vcc"), F("VCC"), F("V")).setValue(F("3.286"));
    // row->addBadgeSensor(F("frequency"), F("Frequency"), F("Hz")).setValue(F("59.869"));
    // row->addBadgeSensor(F("int_temp"), F("Int. Temp"), F("°C")).setValue(F("47.28"));

    for(auto plugin: plugins) {
        if (plugin->hasWebUI()) {
            plugin->createWebUI(webUI);
        }
    }

    // row = &webUI.addRow();
    // row->setExtraClass(JJ(title));
    // row->addGroup(F("Sensors"), false);

    // row = &webUI.addRow();
    // row->addSensor(F("temperature"), F("Temperature"), F("°C")).setValue(F("25.78"));
    // row->addSensor(F("humidity"), F("Humidity"), F("%")).setValue(F("47.23"));
    // row->addSensor(F("pressure"), F("Pressure"), F("hPa")).setValue(F("1023.42"));

    webUI.addValues();
}
