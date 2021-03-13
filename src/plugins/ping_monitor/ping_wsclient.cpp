/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
#include <web_server.h>
#include "ping_monitor.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

WsClientAsyncWebSocket *wsPing = nullptr;

static void _pingMonitorEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    __LDBG_printf("server=%p client=%p ping=%p", server, client, wsPing);
    if (wsPing) {
        WsPingClient::onWsEvent(server, client, (int)type, data, len, arg, WsPingClient::getInstance);
    }
}

void _pingMonitorSetupWebHandler()
{
    if (wsPing) {
        __LDBG_printf("wsPing=%p not null", wsPing);
        return;
    }
    if (Plugins::Ping::getConfig().console) {
        auto server = WebServer::Plugin::getWebServerObject();
        if (server) {
            auto ws = __LDBG_new(WsClientAsyncWebSocket, F("/ping"), &wsPing);
            ws->onEvent(_pingMonitorEventHandler);
            server->addHandler(ws);
            Logger_notice("Web socket for ping running on port %u", System::WebServer::getConfig().getPort());
        }
    }
    else {
        __LDBG_print("web socket disabled");
    }
}

void _pingMonitorShutdownWebSocket()
{
    if (wsPing) {
        wsPing->shutdown();
    }
}

// ------------------------------------------------------------------------
// class WsPingClient

WsPingClient::WsPingClient(AsyncWebSocketClient *client) : WsClient(client)
{
}

WsPingClient::~WsPingClient()
{
    _cancelPing();
}

WsClient *WsPingClient::getInstance(AsyncWebSocketClient *socket)
{
    return __LDBG_new(WsPingClient, socket);
}

void WsPingClient::onText(uint8_t *data, size_t len)
{
    __LDBG_printf("data=%p len=%d", data, len);
    auto client = getClient();
    if (isAuthenticated()) {
        Buffer buffer;

        static constexpr size_t commandLength = constexpr_strlen("+PING ");
        if (len > commandLength && strncmp_P(reinterpret_cast<char *>(data), PSTR("+PING "), commandLength) == 0) {

            buffer.write(data + commandLength, len - commandLength);
            StringVector items;
            explode(buffer.c_str(), ' ', items, 3);

            __LDBG_printf("data=%s strlen=%d items=[%s]", buffer.c_str(), buffer.length(), implode(',', items).c_str());

            if (items.size() == 3) {
                auto count = (int)items[0].toInt();
                auto timeout = (int)items[1].toInt();
                const auto host = items[2];
                _pingMonitorValidateValues(count, timeout);

                _cancelPing();

                // WiFi.hostByName() fails with -5 (INPROGRESS) if called inside onText()

                LoopFunctions::callOnce([this, client, host, count, timeout]() {

                    __LDBG_printf("host=%s count=%d timeout=%d client=%p ping=%p", host.c_str(), count, timeout, client, _ping.get());
                    IPAddress addr;
                    PrintString message;
                    if (_pingMonitorResolveHost(host.c_str(), addr, message)) {

                        if (!_ping) {
                            _ping.reset(new AsyncPing());
                        }

                        __DBG_printf("_ping=%p", &_ping);
                        __DBG_printf("_ping.get()=%p", _ping.get());
                        __DBG_printf("client=%p", &client);

                        _ping->on(true, [client](AsyncPingResponse response) {
                            __LDBG_AsyncPingResponse(true, response);
                            LoopFunctions::callOnce([client, response]() {
                                if (response.answer) {
                                    WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_response), response.size, response.addr.toString().c_str(), response.icmp_seq, response.ttl, response.time));
                                } else {
                                    WsClient::safeSend(wsPing, client, FSPGM(ping_monitor_request_timeout));
                                }
                            });
                            return false;
                        });

                        _ping->on(false, [client](AsyncPingResponse response) {
                            __LDBG_AsyncPingResponse(false, response);
                            // create a copy for callOnce()
                            String mac = mac2String(response.mac);
                            LoopFunctions::callOnce([client, response, mac]() {
                                WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_end_response), response.addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time));
                                if (mac.length()) {
                                    WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_ethernet_detected), mac.c_str()));
                                }
                                WsClient::safeSend(wsPing, client, F("+CLOSE"));
                            });
                            return true;
                        });

                        _pingMonitorBegin(_ping, host.c_str(), addr, count, timeout, message);
                    }

                    // message is set by _pingMonitorResolveHost or _pingMonitorBegin
                    WsClient::safeSend(wsPing, client, message);
                });
            }
        }
    }
}

AsyncPingPtr &WsPingClient::getPing()
{
    return _ping;
}

void WsPingClient::onDisconnect(uint8_t *data, size_t len)
{
    __LDBG_print("disconnect");
    _cancelPing();
}

void WsPingClient::onError(WsErrorType type, uint8_t *data, size_t len)
{
    __LDBG_printf("error type=%u", type);
    _cancelPing();
}

void WsPingClient::_cancelPing()
{
    _pingMonitorSafeCancelPing(_ping);
}

