/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <JsonBuffer.h>
#include <assert.h>
#include "plugins.h"
#include "session.h"
#include "WebUISocket.h"

// #if IOT_ATOMIC_SUN_V2
// #include "../src/plugins/atomic_sun/atomic_sun_v2.h"
// #elif IOT_DIMMER_MODULE
// #include "../src/plugins/dimmer_module/dimmer_plugin.h"
// #endif


#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

PROGMEM_STRING_DEF(webui_socket_uri, "/webui-ws");

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
        auto ws = __LDBG_new(WsClientAsyncWebSocket, FSPGM(webui_socket_uri), &wsWebUI);
        ws->onEvent(webui_socket_event_handler);
        server->addHandler(ws);
        __LDBG_printf("Web socket for UI running on port %u", System::WebServer::getConfig().getPort());
    }
}

void WsWebUISocket::send(AsyncWebSocketClient *client, const JsonUnnamedObject &json)
{
    __LDBG_printf("send json");
    auto server = client->server();
    auto buffer = server->makeBuffer(json.length());
    assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
    client->text(buffer);
}

void WsWebUISocket::broadcast(WsWebUISocket *sender, const JsonUnnamedObject &json)
{
    __LDBG_printf("broadcast sender=%p json", sender);
    if (wsWebUI) {
        auto buffer = wsWebUI->makeBuffer(json.length());
        assert(JsonBuffer(json).fillBuffer(buffer->get(), buffer->length()) == buffer->length());
        __LDBG_printf("buffer=%s", buffer->get());
        WsClient::broadcast(wsWebUI, sender, buffer);
    }
}

void WsWebUISocket::broadcast(WsWebUISocket *sender, uint8_t *buf, size_t len)
{
    if (wsWebUI) {
        auto buffer = wsWebUI->makeBuffer(buf, len, false);
        __LDBG_printf("buffer=%s", buffer->get());
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
    __LDBG_println();
    return __LDBG_new(WsWebUISocket, socket);
}

void WsWebUISocket::onText(uint8_t *data, size_t len)
{
    __LDBG_printf("data=%p len=%d", data, len);
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

        __LDBG_printf("command=%s args=%s", command.c_str(), implode(',', args, argc).c_str());

        if (strcasecmp_P(command.c_str(), PSTR("+get_values")) == 0) {
            sendValues(client);
        }
        else if (strcasecmp_P(command.c_str(), PSTR("+set_state")) == 0) {
            bool state = args[1].toInt() || (strcasecmp_P(args[1].c_str(), SPGM(true)) == 0);
            for(auto plugin: plugins) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], String(), false, state, true);
                }
            }
        }
        else if (strcasecmp_P(command.c_str(), PSTR("+set")) == 0) {
            _sender = this;
            for(auto plugin: plugins) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], args[1], true, false, false);
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

    for(const auto plugin: plugins) {
        if (plugin->hasWebUI()) {
            __LDBG_printf("plugin=%s array_size=%u", plugin->getName_P(), array.size());
            plugin->getValues(array);
        }
    }
    send(client, json);
}

void WsWebUISocket::createWebUIJSON(JsonUnnamedObject &json)
{
    WebUIRoot webUI(json);

    for( const auto plugin: plugins) {
        __LDBG_printf("plugin=%s webui=%u", plugin->getName_P(), plugin->hasWebUI());
        if (plugin->hasWebUI()) {
            plugin->createWebUI(webUI);
        }
    }

    webUI.addValues();
}
