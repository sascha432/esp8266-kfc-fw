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
using Plugins = KFCConfigurationClasses::PluginsType;

WsClientAsyncWebSocket *wsPing = nullptr;

// ------------------------------------------------------------------------
// class WsPingClient

namespace PingMonitor {

    void eventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
    {
        __LDBG_printf("server=%p client=%p ping=%p", server, client, wsPing);
        if (wsPing) {
            PingMonitor::WsPingClient::onWsEvent(server, client, (int)type, data, len, arg, PingMonitor::WsPingClient::getInstance);
        }
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
                    validateValues(count, timeout);

                    _cancelPing();

                    // WiFi.hostByName() fails with -5 (INPROGRESS) if called inside onText()

                    LoopFunctions::callOnce([this, client, host, count, timeout]() {

                        __LDBG_printf("host=%s count=%d timeout=%d client=%p ping=%p", host.c_str(), count, timeout, client, _ping.get());
                        IPAddress addr;
                        PrintString message;
                        if (resolveHost(host.c_str(), addr, message)) {

                            if (!_ping) {
                                _ping.reset(new AsyncPing(), WsPingClient::getDefaultDeleter);
                            }

                            __DBG_printf("_ping=%p", &_ping);
                            __DBG_printf("_ping.get()=%p", _ping.get());
                            __DBG_printf("client=%p", &client);

                            _ping->on(true, [client](AsyncPingResponse response) {
                                __LDBG_AsyncPingResponse(true, response);
                                LoopFunctions::callOnce([client, response]() {
                                    if (response.answer) {
                                        WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_response), response.size, response.addr.toString().c_str(), response.icmp_seq, response.ttl, response.time));
                                    }
                                    else {
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

                            begin(_ping, host.c_str(), addr, count, timeout, message);
                        }

                        // message is set by _pingMonitorResolveHost or _pingMonitorBegin
                        WsClient::safeSend(wsPing, client, message);
                    });
                }
            }
        }
    }

}
