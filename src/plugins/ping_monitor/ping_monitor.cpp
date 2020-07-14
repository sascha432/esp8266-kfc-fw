/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <Buffer.h>
#include "ping_monitor.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"
#include "plugins_menu.h"

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PingMonitorTask::PingMonitorTaskPtr pingMonitorTask;
WsClientAsyncWebSocket *wsPing = nullptr;

void ping_monitor_event_handler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WsPingClient::onWsEvent(server, client, (int)type, data, len, arg, WsPingClient::getInstance);
}

void ping_monitor_install_web_server_hook()
{
    auto server = WebServerPlugin::getWebServerObject();
    if (server) {
        wsPing = new WsClientAsyncWebSocket(F("/ping"));
        wsPing->onEvent(ping_monitor_event_handler);
        server->addHandler(wsPing);
        _debug_printf_P(PSTR("Web socket for ping running on port %u\n"), config._H_GET(Config().http_port));
    }
}

bool ping_monitor_resolve_host(const String &host, IPAddress &addr, PrintString &errorMessage)
{
    if (!host.length()) {
        errorMessage.printf_P(PSTR("ping cancelled"));
        return false;
    }

    if (addr.fromString(host) || WiFi.hostByName(host.c_str(), addr)) {
        _debug_printf_P(PSTR("ping_monitor_resolve_host: resolved host %s = %s\n"), host.c_str(), addr.toString().c_str());
        return true;
    }
    errorMessage.printf_P(PSTR("ping: %s: Name or service not known"), host.c_str());
    return false;
}

void ping_monitor_begin(const AsyncPingPtr &ping, const String &host, IPAddress &addr, int count, int timeout, PrintString &message)
{
    if (count < 1) {
        count = 4;
    }
    if (timeout < 1) {
        timeout = 5000;
    }
    _debug_printf_P(PSTR("addr=%s count=%d timeout=%d\n"), addr.toString().c_str(), count, timeout);
    message.printf_P(PSTR("PING %s (%s) 56(84) bytes of data."), host.c_str(), addr.toString().c_str());
    ping->begin(addr, count, timeout);
}

PROGMEM_STRING_DEF(ping_monitor_response, "%d bytes from %s: icmp_seq=%d ttl=%d time=%ld ms");
PROGMEM_STRING_DEF(ping_monitor_end_response, "Total answer from %s sent %d recevied %d time %ld ms");
PROGMEM_STRING_DEF(ping_monitor_ethernet_detected, "Detected eth address %s");
PROGMEM_STRING_DEF(ping_monitor_request_timeout, "Request timed out.");

WsPingClient::WsPingClient(AsyncWebSocketClient *client) : WsClient(client)
{
}

WsPingClient::~WsPingClient()
{
    _cancelPing();
}

WsClient *WsPingClient::getInstance(AsyncWebSocketClient *socket)
{
    return new WsPingClient(socket);
}

void WsPingClient::onText(uint8_t *data, size_t len)
{
    _debug_printf_P(PSTR("data=%p len=%d\n"), data, len);
    auto client = getClient();
    if (isAuthenticated()) {
        Buffer buffer;

        if (len > 6 && strncmp_P(reinterpret_cast<char *>(data), PSTR("+PING "), 6) == 0) {
            buffer.write(data + 6, len - 6);
            StringVector items;
            explode(buffer.c_str(), ' ', items);

            _debug_printf_P(PSTR("data=%s strlen=%d\n"), buffer.c_str(), buffer.length());

            if (items.size() == 3) {
                auto count = items[1].toInt();
                auto timeout = items[2].toInt();

                _cancelPing();

                // WiFi.hostByName() fails with -5 (INPROGRESS) if called inside onText()
                String host = items[0];

                LoopFunctions::callOnce([this, client, host, count, timeout]() {

                    _debug_printf_P(PSTR("Ping host %s count %d timeout %d\n"), host.c_str(), count, timeout);
                    IPAddress addr;
                    PrintString message;
                    if (ping_monitor_resolve_host(host.c_str(), addr, message)) {

                        _ping->on(true, [client](const AsyncPingResponse &response) {
                            _debug_println(F("_ping.on(true)"));
                            IPAddress addr(response.addr);
                            if (response.answer) {
                                WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_response), response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time));
                            } else {
                                WsClient::safeSend(wsPing, client, FSPGM(ping_monitor_request_timeout));
                            }
                            return false;
                        });

                        _ping->on(false, [client](const AsyncPingResponse &response) {
                            _debug_println(F("_ping.on(false)"));
                            IPAddress addr(response.addr);
                            WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_end_response), addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time));
                            if (response.mac) {
                                WsClient::safeSend(wsPing, client, PrintString(FSPGM(ping_monitor_ethernet_detected), mac2String(response.mac->addr).c_str()));
                            }
                            return true;
                        });

                        ping_monitor_begin(_ping, host.c_str(), addr, count, timeout, message);
                        WsClient::safeSend(wsPing, client, message);
                    }
                    else {
                        WsClient::safeSend(wsPing, client, message);
                    }
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
    _cancelPing();
}

void WsPingClient::onError(WsErrorType type, uint8_t *data, size_t len)
{
    _cancelPing();
}

void WsPingClient::_cancelPing()
{
    _debug_printf_P(PSTR("ping=%p\n"), _ping.get());
    if (_ping) {
#if DEBUG_PING_MONITOR
        _ping->on(true, [](const AsyncPingResponse &response) {
            _debug_println(F("_ping.on(true): callback removed"));
            return false;
        });
        _ping->on(false, [](const AsyncPingResponse &response) {
            _debug_println(F("_ping.on(false): callback removed"));
            return false;
        });
#else
        _ping->on(true, nullptr);
        _ping->on(false, nullptr);
#endif
        _ping->cancel();
    }
}

String ping_monitor_get_translated_host(String host)
{
    host.replace(F("${gateway}"), WiFi.isConnected() ? WiFi.gatewayIP().toString() : emptyString);
    return host;
}

void ping_monitor_loop_function()
{
    if (pingMonitorTask && pingMonitorTask->isNext()) {
        pingMonitorTask->begin();
    }
}

bool ping_monitor_response_handler(const AsyncPingResponse &response)
{
    _debug_println();
    pingMonitorTask->addAnswer(response.answer);
    return false;
}

bool ping_monitor_end_handler(const AsyncPingResponse &response)
{
    _debug_println();
    pingMonitorTask->next();
    return true;
}

PingMonitorTask::PingMonitorTask()
{
}

PingMonitorTask::~PingMonitorTask()
{
    _cancelPing();
}

void PingMonitorTask::setInterval(uint16_t interval)
{
    _interval = interval;
}

void PingMonitorTask::setCount(uint8_t count)
{
    _count = count;
}

void PingMonitorTask::setTimeout(uint16_t timeout)
{
    _timeout = timeout;
}

void PingMonitorTask::clearHosts()
{
    _pingHosts.clear();
}

void PingMonitorTask::addHost(String host)
{
    _debug_printf_P(PSTR("host=%s\n"), host.c_str());
    host.trim();
    if (host.length()) {
        _pingHosts.emplace_back(std::move(host));
    }
}

void PingMonitorTask::addAnswer(bool answer)
{
    _debug_printf_P(PSTR("answer=%d server=%d\n"), answer, _currentServer);
    auto &host = _pingHosts.at(_currentServer);
    if (answer) {
        host.success++;
    } else {
        host.failure++;
    }
}

void PingMonitorTask::next()
{
    _currentServer++;
    _currentServer = _currentServer % _pingHosts.size();
    _nextHost = millis() + (_interval * 1000UL);
    _debug_printf_P(PSTR("server=%d, time=%lu\n"), _currentServer, _nextHost);
}

void PingMonitorTask::begin()
{
    _nextHost = 0;
    String host = ping_monitor_get_translated_host(_pingHosts[_currentServer].host);
    _debug_printf_P(PSTR("host=%s\n"), host.c_str());
    if (host.length() == 0 || !_ping->begin(host.c_str(), _count, _timeout)) {
        _debug_printf_P(PSTR("error: %s\n"), host.c_str());
        next();
    }
}

void PingMonitorTask::printStats(Print &out)
{
    out.printf_P(PSTR("Active, %d second interval" HTML_S(br)), _interval);
    for(auto &host: _pingHosts) {
        out.printf_P(PSTR("%s: %u received, %u lost, %.2f%%" HTML_S(br)), host.host.c_str(), host.success, host.failure, (host.success * 100.0 / (float)(host.success + host.failure)));
    }
}

void PingMonitorTask::start()
{
    _debug_println();
    _cancelPing();
    _currentServer = 0;
    _nextHost = 0;
    if (_pingHosts.size()) {
        _ping.reset(new AsyncPing());
        _ping->on(true, ping_monitor_response_handler);
        _ping->on(false, ping_monitor_end_handler);
        _nextHost = millis() + (_interval * 1000UL);
        _debug_printf_P(PSTR("next=%lu\n"), _nextHost);
        LoopFunctions::add(ping_monitor_loop_function);
    } else {
        _debug_printf_P(PSTR("no hosts\n"));
    }
}

void PingMonitorTask::stop()
{
    _debug_printf_P(PSTR("PingMonitorTask::stop()\n"));
    _cancelPing();
    clearHosts();
}

void PingMonitorTask::_cancelPing()
{
    _debug_printf_P(PSTR("ping=%p\n"), _ping.get());
    if (_ping) {
        LoopFunctions::remove(ping_monitor_loop_function);
        _ping->cancel();
        _ping = nullptr;
    }
}

void ping_monitor_setup()
{
    ping_monitor_install_web_server_hook();

    pingMonitorTask = nullptr;
    auto config = ::config._H_GET(Config().ping.config);

    if (config.interval && config.count) {

        _debug_printf_P(PSTR("setting up PingMonitorTask interval=%u count=%d timeout=%d\n"), config.interval, config.count, config.timeout);

        pingMonitorTask.reset(new PingMonitorTask());
        pingMonitorTask->setInterval(config.interval);
        pingMonitorTask->setCount(config.count);
        pingMonitorTask->setTimeout(config.timeout);

        for(uint8_t i = 0; i < 4; i++) {
            pingMonitorTask->addHost(Config_Ping::getHost(i));
        }

        pingMonitorTask->start();
    }
}

class PingMonitorPlugin : public PluginComponent {
public:
    PingMonitorPlugin() {
        REGISTER_PLUGIN(this);
    }
    PGM_P getName() const {
        return PSTR("pingmon");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Ping Monitor");
    }

    virtual PriorityType getSetupPriority() const override {
        return PriorityType::PING_MONITOR;
    }
    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override {
        return plugin->nameEquals(FSPGM(http));
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuType getMenuType() const override {
        return MenuType::CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(getFriendlyName(), F("ping_monitor.html"), navMenu.config);
        bootstrapMenu.addSubMenu(F("Ping Remote Host"), F("ping.html"), navMenu.util);
    }

    virtual PGM_P getConfigureForm() const override {
        return PSTR("ping_monitor");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

static PingMonitorPlugin plugin;

void PingMonitorPlugin::setup(SetupModeType mode)
{
    ping_monitor_setup();
}

void PingMonitorPlugin::reconfigure(PGM_P source)
{
    ping_monitor_setup();
}

void PingMonitorPlugin::shutdown()
{
    pingMonitorTask = nullptr;
    wsPing->shutdown();
}

void PingMonitorPlugin::getStatus(Print &output)
{
    if (pingMonitorTask) {
        pingMonitorTask->printStats(output);
    }
    else {
        output.print(FSPGM(Disabled));
    }
}

void PingMonitorPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    auto gateway = F("${gateway}");

    form.add(F("ping_host1"), _H_STR_VALUE(Config().ping.host1));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add(F("ping_host2"), _H_STR_VALUE(Config().ping.host2));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add(F("ping_host3"), _H_STR_VALUE(Config().ping.host3));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));

    form.add(F("ping_host4"), _H_STR_VALUE(Config().ping.host4));
    form.addValidator((new FormValidHostOrIpValidator(true))->addAllowString(gateway));


    form.add<uint16_t>(F("ping_interval"), _H_STRUCT_VALUE(Config().ping.config, interval));
    form.addValidator(new FormRangeValidator(0, 65535));

    form.add<uint8_t>(F("ping_count"), _H_STRUCT_VALUE(Config().ping.config, count));
    form.addValidator(new FormRangeValidator(0, 255));

    form.add<uint16_t>(F("ping_timeout"), _H_STRUCT_VALUE(Config().ping.config, timeout));
    form.addValidator(new FormRangeValidator(0, 65535));

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PING, "PING", "<target[,count=4[,timeout=5000]]>", "Ping host or IP address");

void PingMonitorPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(PING), getName());
}

bool PingMonitorPlugin::atModeHandler(AtModeArgs &args)
{
    static AsyncPingPtr _ping;

    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PING))) {

        if (args.requireArgs(1, 3)) {
            auto host = args.get(0);
            IPAddress addr;
            PrintString message;
            if (_ping) {
                _debug_println(F("ping_monitor: previous ping cancelled"));
                _ping->cancel();
            }
            auto &serial = args.getStream();
            if (ping_monitor_resolve_host(host, addr, message)) {
                int count = args.toInt(1, 4);
                int timeout = args.toInt(2, 5000);
                if (!_ping) {
                    _ping.reset(new AsyncPing());
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
