/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <JsonBuffer.h>
#include <assert.h>
#include "plugins.h"
#include "session.h"
#include "WebUISocket.h"

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

PROGMEM_STRING_DEF(webui_socket_uri, "/webui-ws");

WsClientAsyncWebSocket *WebUISocket::_server;
WebUISocket *WebUISocket::_sender;

void webui_socket_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WebUISocket::onWsEvent(server, client, (int)type, data, len, arg, WebUISocket::createInstance);
}

void WebUISocket::setup(AsyncWebServer *server)
{
    auto ws = __LDBG_new(WsClientAsyncWebSocket, FSPGM(webui_socket_uri), &_server);
    ws->onEvent(webui_socket_event_handler);
    server->addHandler(ws);
    __LDBG_printf("Web socket for UI running on port %u", System::WebServer::getConfig().getPort());
}

void WebUISocket::onText(uint8_t *data, size_t len)
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
            for(auto plugin: PluginComponents::Register::getPlugins()) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], String(), false, state, true);
                }
            }
        }
        else if (strcasecmp_P(command.c_str(), PSTR("+set")) == 0) {
            _sender = this;
            for(auto plugin: PluginComponents::Register::getPlugins()) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], args[1], true, false, false);
                }
            }
            _sender = nullptr;
        }
    }
}

void WebUISocket::sendValues(AsyncWebSocketClient *client)
{
    JsonUnnamedObject json(1);

    json.add(JJ(type), JJ(ue));
    auto &array = json.addArray(JJ(events));

    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->hasWebUI()) {
            __LDBG_printf("plugin=%s array_size=%u", plugin->getName_P(), array.size());
            plugin->getValues(array);
        }
    }
    send(client, json);
}

void WebUISocket::createWebUIJSON(JsonUnnamedObject &json)
{
    WebUIRoot webUI(json);

    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        __LDBG_printf("plugin=%s webui=%u", plugin->getName_P(), plugin->hasWebUI());
        if (plugin->hasWebUI()) {
            plugin->createWebUI(webUI);
        }
    }

    webUI.addValues();
}
