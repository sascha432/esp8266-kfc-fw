/**
  Author: sascha_lammers@gmx.de
*/

#if PING_MONITOR

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <KFCForms.h>
#include <Buffer.h>
#include "ping_monitor.h"
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "EventScheduler.h"
#include "LoopFunctions.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

const String ping_monitor_get_status() {
    if (config._H_GET(Config().ping.interval) && config._H_GET(Config().ping.count)) {
        PrintHtmlEntitiesString out;
        out.printf_P(PSTR("Active, %d minute%s interval, paket loss since reboot %.2f%, avg. ping response time %d ms"), config._H_GET(Config().ping.interval), config._H_GET(Config().ping.interval) == 1 ? "" : "s", 100.0, 0);
        return out;
    } else {
        return SPGM(Disabled);
    }
}

void ping_monitor_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsPingClient::onWsEvent(server, client, (int)type, data, len, arg, WsPingClient::getInstance);
}

void ping_monitor_install_web_server_hook() {
    if (get_web_server_object()) {
        AsyncWebSocket *ws_ping = _debug_new AsyncWebSocket(F("/ping"));
        ws_ping->onEvent(ping_monitor_event_handler);
        web_server_add_handler(ws_ping);
        _debug_printf_P(PSTR("Web socket for ping running on port %u\n"), config._H_GET(Config().http_port));
    }
}

WsClient *WsPingClient::getInstance(AsyncWebSocketClient *socket) {

    _debug_println(F("WsPingClient::getInstance()"));
    WsClient *wsClient = WsClientManager::getWsClientManager()->getWsClient(socket);
    if (!wsClient) {
        wsClient = _debug_new WsPingClient(socket);
    }
    return wsClient;
}

bool ping_monitor_resolve_host(char *host, IPAddress &addr, PrintString &errorMessage) {

    if (addr.fromString(host) || WiFi.hostByName(host, addr)) {
        _debug_printf_P(PSTR("ping_monitor_resolve_host: resolved host %s = %s\n"), host, addr.toString().c_str());
        return true;
    }
    errorMessage.printf_P(PSTR("ping: %s: Name or service not known"), host);
    return false;
}

void ping_monitor_begin(AsyncPing *ping, char *host, IPAddress &addr, int count, int timeout, PrintString &message) {
    if (count < 1) {
        count = 4;
    }
    if (timeout < 1) {
        timeout = 5000;
    }
    _debug_printf_P(PSTR("ping->begin(%s, %d, %d)\n"), addr.toString().c_str(), count, timeout);
    message.printf_P(PSTR("PING %s (%s) 56(84) bytes of data."), host, addr.toString().c_str());
    ping->begin(addr, count, timeout);
}

PROGMEM_STRING_DEF(ping_monitor_response, "%d bytes from %s: icmp_seq=%d ttl=%d time=%ld ms");
PROGMEM_STRING_DEF(ping_monitor_end_response, "Total answer from %s sent %d recevied %d time %ld ms");
PROGMEM_STRING_DEF(ping_monitor_ethernet_detected, "Detected eth address %s");
PROGMEM_STRING_DEF(ping_monitor_request_timeout, "Request timed out.");

void WsPingClient::onText(uint8_t *data, size_t len) {

    _debug_printf_P(PSTR("WsPingClient::onText(%p, %d)\n"), data, len);
    auto client = getClient();
    if (isAuthenticated()) {
        Buffer buffer;

        if (len > 6 && strncmp_P(reinterpret_cast<char *>(data), PSTR("+PING "), 6) == 0 && buffer.reserve(len + 1 - 6)) {
            char *buf = buffer.getChar();
            len -= 6;
            strncpy(buf, reinterpret_cast<char *>(data) + 6, len)[len] = 0;

            _debug_printf_P(PSTR("WsPingClient::onText(): data='%s', strlen=%d\n"), buf, strlen(buf));

            const char *delimiters = " ";
            char *count_str = strtok(buf, delimiters);
            char *timeout_str = strtok(nullptr, delimiters);
            char *host = strtok(nullptr, delimiters);

            if (count_str && timeout_str && host) {
                int count = atoi(count_str);
                int timeout = atoi(timeout_str);

                // TODO this is pretty nasty. maybe the web socket is blocking the dns resolver
                // WiFi.hostByName() fails with -5 (INPROGRESS) if called inside onText()
                // assuming that there cannot be any onDisconnect or onError event before the main loop, otherwise "this" won't be valid anymore = crash
                host = strdup(host);
                if (host) {
                    // call in the main loop
                    LoopFunctions::add([this, client, host, count, timeout]() {

                        _debug_printf_P(PSTR("Ping host %s count %d timeout %d\n"), host, count, timeout);
                        IPAddress addr;
                        PrintString message;
                        if (ping_monitor_resolve_host(host, addr, message)) {

                            _ping.on(true, [client](const AsyncPingResponse &response) {
                                auto mgr = WsClientManager::getWsClientManager();
                                PrintString str;
                                IPAddress addr(response.addr);
                                if (response.answer) {
                                    str.printf_P(SPGM(ping_monitor_response), response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
                                    mgr->safeSend(client, str);
                                } else {
                                    mgr->safeSend(client, FSPGM(ping_monitor_request_timeout));
                                }
                                return false;
                            });

                            _ping.on(false, [client](const AsyncPingResponse &response) {
                                auto mgr = WsClientManager::getWsClientManager();
                                PrintString str;
                                IPAddress addr(response.addr);
                                str.printf_P(SPGM(ping_monitor_end_response), addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time);
                                mgr->safeSend(client, str);
                                if (response.mac) {
                                    str.printf_P(SPGM(ping_monitor_ethernet_detected), mac2String(response.mac->addr).c_str());
                                    mgr->safeSend(client, str);
                                }
                                return true;
                            });

                            ping_monitor_begin(&_ping, host, addr, count, timeout, message);
                            WsClientManager::getWsClientManager()->safeSend(client, message);

                        } else {
                            WsClientManager::getWsClientManager()->safeSend(client, message);
                        }

                        free(host);

                        LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(host)); // host is used to identify the lambda function

                    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(host));
                }
            }
        }
    }
}

WsPingClient::WsPingClient( AsyncWebSocketClient *socket) : WsClient(socket) {
}

WsPingClient::~WsPingClient() {
    _cancelPing();
}

AsyncPing &WsPingClient::getPing() {
    return _ping;
}

void WsPingClient::onDisconnect(uint8_t *data, size_t len) {
    _cancelPing();
}

void WsPingClient::onError(WsErrorType type, uint8_t *data, size_t len) {
    _cancelPing();
}

void WsPingClient::_cancelPing() {
    _debug_println(F("WsPingClient::_cancelPing()"));
    _ping.cancel();
}

String WsPingClient::_getHost(uint8_t num) const {
    const ConfigurationParameter::Handle_t handle[] = { _H(Config().ping.host1), _H(Config().ping.host2), _H(Config().ping.host3) };
    if (num >= sizeof(handle) / sizeof(handle[0])) {
        return _sharedEmptyString;
    }
    String str = config.getString(handle[num]);
    str.replace(F("${gateway}"), WiFi.isConnected() ? WiFi.gatewayIP().toString() : _sharedEmptyString);
    return str;
}

void ping_monitor_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    form.add<sizeof Config().ping.host1>(F("ping_host1"), config._H_W_STR(Config().ping.host1));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ping.host2>(F("ping_host2"), config._H_W_STR(Config().ping.host2));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ping.host3>(F("ping_host3"), config._H_W_STR(Config().ping.host3));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ping.host4>(F("ping_host4"), config._H_W_STR(Config().ping.host4));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<uint16_t>(F("ping_interval"), &config._H_W_GET(Config().ping.interval));
    form.addValidator(new FormRangeValidator(0, 65535));

    form.add<uint8_t>(F("ping_count"), &config._H_W_GET(Config().ping.count));
    form.addValidator(new FormRangeValidator(0, 255));

    form.add<uint16_t>(F("ping_timeout"), &config._H_W_GET(Config().ping.timeout));
    form.addValidator(new FormRangeValidator(0, 65535));

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PING, "PING", "<target[,count=4[,timeout=5000]]>", "Ping host or IP address");

bool ping_monitor_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {
    static AsyncPing *_ping = nullptr;

    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PING));

    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(PING))) {

        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        } else {
            char *host = argv[0];
            IPAddress addr;
            PrintString message;
            if (ping_monitor_resolve_host(host, addr, message)) {
                int count = 0;
                int timeout = 0;
                if (argc >= 2) {
                    count = atoi(argv[1]);
                    if (argc >= 3) {
                        timeout = atoi(argv[2]);
                    }
                }
                if (_ping) {
                    _debug_println(F("ping_monitor: previous ping cancelled"));
                    _ping->cancel();
                } else {
                    _ping = new AsyncPing();
                    _ping->on(true, [&serial](const AsyncPingResponse &response) {
                        IPAddress addr(response.addr);
                        if (response.answer) {
                            serial.printf_P(SPGM(ping_monitor_response), response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
                            serial.println();
                        } else {
                            serial.println(FSPGM(ping_monitor_request_timeout));
                        }
                        return false;
                    });
                    _ping->on(false, [&serial](const AsyncPingResponse &response) {
                        IPAddress addr(response.addr);
                        serial.printf_P(SPGM(ping_monitor_end_response), addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time);
                        serial.println();
                        if (response.mac) {
                            serial.printf_P(SPGM(ping_monitor_ethernet_detected), mac2String(response.mac->addr).c_str());
                            serial.println();
                        }
                        delete _ping;
                        _ping = nullptr;
                        return true;
                    });
                }
                ping_monitor_begin(_ping, host, addr, count, timeout, message);
                serial.println(message);

            } else {
                serial.println(message);
            }
        }
        return true;

    }
    return false;
}

#endif

PROGMEM_STRING_DECL(plugin_config_name_http);

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ pingmon,
/* setupPriority            */ 100,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ ping_monitor_install_web_server_hook,
/* statusTemplate           */ ping_monitor_get_status,
/* configureForm            */ ping_monitor_create_settings_form,
/* reconfigurePlugin        */ ping_monitor_install_web_server_hook,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ ping_monitor_at_mode_command_handler
);

#endif
