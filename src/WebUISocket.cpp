/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintString.h>
#include <assert.h>
#if ESP8266
#include <interrupts.h>
#endif
#include "plugins.h"
#include "session.h"
#include "WebUISocket.h"

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
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
    auto ws = new WsClientAsyncWebSocket(FSPGM(webui_socket_uri), &_server);
    ws->onEvent(webui_socket_event_handler);
    server->addHandler(ws);
    __LDBG_printf("Web socket for UI running on port %u", System::WebServer::getConfig().getPort());
}

void WebUISocket::onText(uint8_t *data, size_t len)
{
    __LDBG_printf("data=%p len=%d", data, len);
    if (isAuthenticated()) {
        ScopeCounter<volatile uint16_t>(AsyncWebServer::_requestCounter);
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

        if (command.equalsIgnoreCase(F("+get_values"))) {
            sendValues(client);
        }
        else if (command.equalsIgnoreCase(F("+set_state"))) {
            bool state = args[1].toInt() || (strcasecmp_P(args[1].c_str(), SPGM(true)) == 0);
            for(auto plugin: PluginComponents::Register::getPlugins()) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], String(), false, state, true);
                }
            }
        }
        else if (command.equalsIgnoreCase(F("+set"))) {
            _sender = this;
            for(auto plugin: PluginComponents::Register::getPlugins()) {
                if (plugin->hasWebUI()) {
                    plugin->setValue(args[0], args[1], true, false, false);
                }
            }
            _sender = nullptr;
        }
        else if (command.equalsIgnoreCase(F("+get"))) {
            _sender = this;
            for(auto plugin: PluginComponents::Register::getPlugins()) {
                if (plugin->hasWebUI()) {
                    String value;
                    bool state;
                    if (plugin->getValue(args[0], value, state)) {
                        sendValue(client, FPSTR(args[0].c_str()), value, state);
                    }
                }
            }
            _sender = nullptr;
        }

    }
}

void WebUISocket::sendValues(AsyncWebSocketClient *client)
{
    ScopeCounter<volatile uint16_t>(AsyncWebServer::_responseCounter);
    WebUINS::Events events;
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->hasWebUI()) {
            plugin->getValues(events);
        }
    }
    if (events.hasAny()) {
        send(client, WebUINS::UpdateEvents(events));
    }
}

void WebUISocket::sendValue(AsyncWebSocketClient *client, const __FlashStringHelper *id, const String &value, bool state)
{
    send(client, WebUINS::UpdateEvents(WebUINS::Events(WebUINS::Values(id, value, state))));
}

WebUINS::Root WebUISocket::createWebUIJSON()
{
    WebUINS::Root webUI;

    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        // __DBG_printf("plugin=%s webui=%u", plugin->getName_P(), plugin->hasWebUI());
        if (plugin->hasWebUI()) {
            plugin->createWebUI(webUI);
        }
    }

    webUI.addValues();
    return webUI;
}
