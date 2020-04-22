/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <JsonBuffer.h>
#include <assert.h>
#include "plugins.h"
#include "session.h"
#include "WebUISocket.h"

#if IOT_DIMMER_MODULE
#include "./plugins/dimmer_module/dimmer_module.h"
#endif

#if IOT_ATOMIC_SUN_V2
#include "./plugins/atomic_sun/atomic_sun_v2.h"
#endif

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(webui_socket_uri, "/webui_ws");

WsClientAsyncWebSocket *wsWebUI = nullptr;
WsWebUISocket *WsWebUISocket::_sender = nullptr;

void webui_socket_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WsWebUISocket::onWsEvent(server, client, (int)type, data, len, arg, WsWebUISocket::getInstance);
}

void WsWebUISocket::setup()
{
    auto server = WebServerPlugin::getWebServerObject();
    if (server) {
        wsWebUI = new WsClientAsyncWebSocket(FSPGM(webui_socket_uri));
        wsWebUI->onEvent(webui_socket_event_handler);
        server->addHandler(wsWebUI);
        _debug_printf_P(PSTR("Web socket for UI running on port %u\n"), config._H_GET(Config().http_port));
    }
}

void WsWebUISocket::send(AsyncWebSocketClient *client, JsonUnnamedObject &json)
{
    auto server = client->server();
    auto buffer = server->makeBuffer(json.length());
    assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
    client->text(buffer);
}

void WsWebUISocket::broadcast(WsWebUISocket *sender, JsonUnnamedObject &json)
{
    if (wsWebUI) {
        auto buffer = wsWebUI->makeBuffer(json.length());
        assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
        _debug_printf_P(PSTR("buffer=%s\n"), buffer->get());
        WsClient::broadcast(wsWebUI, sender, buffer);
    }
}

WsWebUISocket *WsWebUISocket::getSender()
{
    return _sender;
}

WsClientAsyncWebSocket *WsWebUISocket::getWsWebUI()
{
    return wsWebUI;
}

WsClient *WsWebUISocket::getInstance(AsyncWebSocketClient *socket)
{
    _debug_println();
    return new WsWebUISocket(socket);
}


void WsWebUISocket::onText(uint8_t *data, size_t len)
{
    _debug_printf_P(PSTR("data=%p len=%d\n"), data, len);
    if (isAuthenticated()) {
        auto client = getClient();
        String command;
        std::array<String, 4> args;
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
            if (++argc == args.size()) {
                break;
            }
        }

        _debug_printf_P(PSTR("command=%s args=%s\n"), command.c_str(), implode(',', args, argc).c_str());

        if (strcasecmp_P(command.c_str(), PSTR("+get_values")) == 0) {
            sendValues(client);
        }
        else if (strcasecmp_P(command.c_str(), PSTR("+set_state")) == 0) {
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

void WsWebUISocket::sendValues(AsyncWebSocketClient *client)
{
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

void WsWebUISocket::createWebUIJSON(JsonUnnamedObject &json)
{
    WebUI webUI(json);

    // auto row = &webUI.addRow();
    // row->setExtraClass(F("webuicomponent-top"));
    // row->setAlignment(WebUIRow::CENTER);

    // row->addBadgeSensor(F("temperature"), F("Temperature"), F("\u00b0C")).setValue(F("25.78"));
    // row->addBadgeSensor(F("humidity"), F("Humidity"), F("%")).setValue(F("47.23"));
    // row->addBadgeSensor(F("pressure"), F("Pressure"), F("hPa")).setValue(F("1023.42"));
    // row->addBadgeSensor(F("vcc"), F("VCC"), F("V")).setValue(F("3.286"));
    // row->addBadgeSensor(F("frequency"), F("Frequency"), F("Hz")).setValue(F("59.869"));
    // row->addBadgeSensor(F("int_temp"), F("Int. Temp"), F("\u00b0C")).setValue(F("47.28"));

    for(auto plugin: plugins) {
        if (plugin->hasWebUI()) {
            plugin->createWebUI(webUI);
        }
    }

    webUI.addValues();
}
