/**
  Author: sascha_lammers@gmx.de
*/

#if PING_MONITOR

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <Buffer.h>
#include "ping_monitor.h"
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PingMonitorTask *pingMonitorTask = nullptr;
AsyncWebSocket *wsPing = nullptr;

void ping_monitor_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    WsPingClient::onWsEvent(server, client, (int)type, data, len, arg, WsPingClient::getInstance);
}

void ping_monitor_install_web_server_hook() {
    if (get_web_server_object()) {
        wsPing = _debug_new AsyncWebSocket(F("/ping"));
        wsPing->onEvent(ping_monitor_event_handler);
        web_server_add_handler(wsPing);
        _debug_printf_P(PSTR("Web socket for ping running on port %u\n"), config._H_GET(Config().http_port));
    }
}

bool ping_monitor_resolve_host(char *host, IPAddress &addr, PrintString &errorMessage) {

    if (!*host || !strcmp_P(host, SPGM(0))) {
        errorMessage.printf_P(PSTR("ping cancelled"));
        return false;
    }

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

WsPingClient::WsPingClient(AsyncWebSocketClient *client) : WsClient(client),  _loopFunction(nullptr) {
}

WsPingClient::~WsPingClient() {
    _cancelPing();
}

WsClient *WsPingClient::getInstance(AsyncWebSocketClient *socket) {
    return _debug_new WsPingClient(socket);
}

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

                _cancelPing();

                // WiFi.hostByName() fails with -5 (INPROGRESS) if called inside onText()
                host = strdup(host);
                if (host) {
                    // host is used to identify the lambda function
                    _loopFunction = host;

                    // we need to create a new instance that survives the lifetime of WsPingClient, since
                    // the AsyncPing timer does not get cancelled when AsyncPing gets destroyed and probably
                    // calling send_packet() with ping_pcb = nullptr causing a crash
                    // _ping = new AsyncPing();
                    // auto ping = _ping;

                    // called in the main loop
                    LoopFunctions::add([this, client, host, count, timeout]() {

                        _debug_printf_P(PSTR("Ping host %s count %d timeout %d\n"), host, count, timeout);
                        IPAddress addr;
                        PrintString message;
                        if (ping_monitor_resolve_host(host, addr, message)) {

                            _ping.on(true, [client](const AsyncPingResponse &response) {
                                _debug_println(F("_ping.on(true)"));
                                IPAddress addr(response.addr);
                                if (response.answer) {
                                    WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_response), response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time));
                                } else {
                                    WsClient::safeSend(wsPing, client, FSPGM(ping_monitor_request_timeout));
                                }
                                return false;
                            });

                            _ping.on(false, [client](const AsyncPingResponse &response) {
                                _debug_println(F("_ping.on(false)"));
                                IPAddress addr(response.addr);
                                WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_end_response), addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time));
                                if (response.mac) {
                                    WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_ethernet_detected), mac2String(response.mac->addr).c_str()));
                                }
                                return true;
                            });

                            ping_monitor_begin(&_ping, host, addr, count, timeout, message);
                            WsClient::safeSend(wsPing, client, message);

                        } else {
                            WsClient::safeSend(wsPing, client, message);
                        }

                        free(host);

                        _loopFunction = nullptr;
                        LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(host));

                    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(host));
                }
            }
        }
    }
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
    _debug_printf_P(PSTR("WsPingClient::_cancelPing(): _loopFunction=%p\n"), _loopFunction);
    if (_loopFunction) {
        LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(_loopFunction));
        free(_loopFunction);
        _loopFunction = nullptr;
    }
#if DEBUG_PING_MONITOR
    _ping.on(true, [](const AsyncPingResponse &response) {
        _debug_println(F("_ping.on(true): callback removed"));
        return false;
    });
    _ping.on(false, [](const AsyncPingResponse &response) {
        _debug_println(F("_ping.on(false): callback removed"));
        return false;
    });
#else
    _ping.on(true, nullptr);
    _ping.on(false, nullptr);
#endif
    _ping.cancel();
}

const char *ping_monitor_get_host(uint8_t num) {
    const ConfigurationParameter::Handle_t handles[] = { _H(Config().ping.host1), _H(Config().ping.host2), _H(Config().ping.host3) };
    if (num >= sizeof(handles) / sizeof(handles[0])) {
        return _sharedEmptyString.c_str();
    }
    return config.getString(handles[num]);
}

String ping_monitor_get_translated_host(String host) {
    host.replace(F("${gateway}"), WiFi.isConnected() ? WiFi.gatewayIP().toString() : _sharedEmptyString);
    return host;
}

void ping_monitor_loop_function() {
    if (pingMonitorTask && pingMonitorTask->isNext()) {
        pingMonitorTask->begin();
    }
}

bool ping_monitor_response_handler(const AsyncPingResponse &response) {
    _debug_println(F("ping_monitor_response_handler"));
    pingMonitorTask->addAnswer(response.answer);
    return false;
}

bool ping_monitor_end_handler(const AsyncPingResponse &response) {
    _debug_println(F("ping_monitor_end_handler"));
    pingMonitorTask->next();
    return true;
}

PingMonitorTask::PingMonitorTask() {
    _ping = nullptr;
}

PingMonitorTask::~PingMonitorTask() {
    _cancelPing();
}

void PingMonitorTask::setInterval(uint16_t interval) {
    _interval = interval;
}

void PingMonitorTask::setCount(uint8_t count) {
    _count = count;
}

void PingMonitorTask::setTimeout(uint16_t timeout) {
    _timeout = timeout;
}

void PingMonitorTask::clearHosts() {
    _pingHosts.clear();
}

void PingMonitorTask::addHost(const char *host) {
    if (*host) {
        _debug_printf_P(PSTR("PingMonitorTask::addHost(%s)\n"), host);
        _pingHosts.push_back({ host, 0, 0 });
    }
}

void PingMonitorTask::addAnswer(bool answer) {
    _debug_printf_P(PSTR("PingMonitorTask::addAnswer(%d): server=%d\n"), answer, _currentServer);
    auto &host = _pingHosts.at(_currentServer);
    if (answer) {
        host.success++;
    } else {
        host.failure++;
    }
}

void PingMonitorTask::next() {
    _currentServer++;
    _currentServer = _currentServer % _pingHosts.size();
    _nextHost = millis() + (_interval * 1000UL);
    _debug_printf_P(PSTR("PingMonitorTask::next(): server=%d, time=%lu\n"), _currentServer, _nextHost);
}

void PingMonitorTask::begin() {
    _nextHost = 0;
    String host = ping_monitor_get_translated_host(_pingHosts[_currentServer].host);
    _debug_printf_P(PSTR("PingMonitorTask::begin(): %s\n"), host.c_str());
    if (host.length() == 0 || !_ping->begin(host.c_str(), _count, _timeout)) {
        _debug_printf_P(PSTR("PingMonitorTask::begin(): error: %s\n"), host.c_str());
        next();
    }
}

void PingMonitorTask::printStats(Print &out) {

    out.printf_P(PSTR("Active, %d second interval" HTML_S(br)), _interval);
    for(auto &host: _pingHosts) {
        out.printf_P(PSTR("%s: %u received, %u lost, %.2f%%" HTML_S(br)), host.host.c_str(), host.success, host.failure, (host.success * 100.0 / (float)(host.success + host.failure)));
    }
}

void PingMonitorTask::start() {
    _cancelPing();
    _currentServer = 0;
    _nextHost = 0;
    if (_pingHosts.size()) {
        _ping = new AsyncPing();
        _ping->on(true, ping_monitor_response_handler);
        _ping->on(false, ping_monitor_end_handler);
        _nextHost = millis() + (_interval * 1000UL);
        _debug_printf_P(PSTR("PingMonitorTask::start(): next=%lu\n"), _nextHost);
        LoopFunctions::add(ping_monitor_loop_function);
    } else {
        _debug_printf_P(PSTR("PingMonitorTask::start(): no hosts\n"));
    }
}

void PingMonitorTask::stop() {
    _debug_printf_P(PSTR("PingMonitorTask::stop()\n"));
    _cancelPing();
    clearHosts();
}

void PingMonitorTask::_cancelPing() {
    if (_ping) {
        _debug_printf_P(PSTR("PingMonitorTask::_cancelPing()\n"));
        LoopFunctions::remove(ping_monitor_loop_function);
        _ping->cancel();
        delete _ping;
        _ping = nullptr;
    }
}

void ping_monitor_setup() {
    ping_monitor_install_web_server_hook();

    if (pingMonitorTask) {
        delete pingMonitorTask;
        pingMonitorTask = nullptr;
    }

    if (config._H_GET(Config().ping.interval) && config._H_GET(Config().ping.count)) {

        _debug_printf_P(PSTR("ping_monitor_setup(): Setting up PingMonitorTask\n"));

        pingMonitorTask = new PingMonitorTask();
        pingMonitorTask->setInterval(config._H_GET(Config().ping.interval));
        pingMonitorTask->setCount(config._H_GET(Config().ping.count));
        pingMonitorTask->setTimeout(config._H_GET(Config().ping.timeout));

        for(uint8_t i = 0; i < 4; i++) {
            pingMonitorTask->addHost(ping_monitor_get_host(i));
        }

        pingMonitorTask->start();
    }
}

class PingMonitorPlugin : public PluginComponent {
public:
    PingMonitorPlugin() {
        register_plugin(this);
    }
    PGM_P getName() const;
    PluginPriorityEnum_t getSetupPriority() const override;
    void setup(PluginSetupMode_t mode) override;
    void reconfigure(PGM_P source) override;
    bool hasReconfigureDependecy(PluginComponent *plugin) const override;
    bool hasStatus() const override;
    const String getStatus() override;
    bool canHandleForm(const String &formName) const override;
    void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
#if AT_MODE_SUPPORTED
    bool hasAtMode() const override;
    void atModeHelpGenerator() override;
    bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif
};

static PingMonitorPlugin plugin;

PGM_P PingMonitorPlugin::getName() const {
    return PSTR("pingmon");
}

PingMonitorPlugin::PluginPriorityEnum_t PingMonitorPlugin::getSetupPriority() const {
    return (PluginPriorityEnum_t)100;
}

void PingMonitorPlugin::setup(PluginSetupMode_t mode) {
    ping_monitor_setup();
}

void PingMonitorPlugin::reconfigure(PGM_P source) {
    ping_monitor_setup();
}

bool PingMonitorPlugin::hasReconfigureDependecy(PluginComponent *plugin) const {
    return plugin->nameEquals(F("http"));
}

bool PingMonitorPlugin::hasStatus() const {
    return true;
}

const String PingMonitorPlugin::getStatus() {
    if (pingMonitorTask) {
        PrintHtmlEntitiesString out;
        pingMonitorTask->printStats(out);
        return out;
    } else {
        return FSPGM(Disabled);
    }
}

bool PingMonitorPlugin::canHandleForm(const String &formName) const {
    return strcmp_P(formName.c_str(), PSTR("ping_monitor")) == 0;
}

void PingMonitorPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

    const __FlashStringHelper *gateway = F("${gateway}");

    form.add<sizeof Config().ping.host1>(F("ping_host1"), config._H_W_STR(Config().ping.host1));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add<sizeof Config().ping.host2>(F("ping_host2"), config._H_W_STR(Config().ping.host2));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add<sizeof Config().ping.host3>(F("ping_host3"), config._H_W_STR(Config().ping.host3));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add<sizeof Config().ping.host4>(F("ping_host4"), config._H_W_STR(Config().ping.host4));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

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

bool PingMonitorPlugin::hasAtMode() const {
    return true;
}

void PingMonitorPlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PING));
}

bool PingMonitorPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    static AsyncPing *_ping = nullptr;

    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(PING))) {

        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        } else {
            char *host = argv[0];
            IPAddress addr;
            PrintString message;
            if (_ping) {
                _debug_println(F("ping_monitor: previous ping cancelled"));
                _ping->cancel();
            }
            if (ping_monitor_resolve_host(host, addr, message)) {
                int count = 0;
                int timeout = 0;
                if (argc >= 2) {
                    count = atoi(argv[1]);
                    if (argc >= 3) {
                        timeout = atoi(argv[2]);
                    }
                }
                if (!_ping) {
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

#endif
