/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <KFCJson.h>
#include <Buffer.h>
#include "ping_monitor.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "Utility/ProgMemHelper.h"
#include <stl_ext/algorithm.h>

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AUTO_STRING_DEF(ping_monitor_response, "%d bytes from %s: icmp_seq=%d ttl=%d time=%u ms")
AUTO_STRING_DEF(ping_monitor_end_response, "Total answer from %s sent %d recevied %d time %u ms")
AUTO_STRING_DEF(ping_monitor_ethernet_detected, "Detected eth address %s")
AUTO_STRING_DEF(ping_monitor_request_timeout, "Request timed out.")
AUTO_STRING_DEF(ping_monitor_service_status, "Ping monitor service has been %s")
AUTO_STRING_DEF(ping_monitor_ping_for_hostname_failed, "Pinging %s failed")
AUTO_STRING_DEF(ping_monitor_cancelled, "Ping cancelled")
AUTO_STRING_DEF(ping_monitor_service, "Ping Monitor Service")

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

bool PingMonitor::resolveHost(const String &host, IPAddress &addr, PrintString &errorMessage)
{
    if (!host.length()) {
        errorMessage.print(FSPGM(ping_monitor_cancelled));
        return false;
    }

    // do not allow INADDR_NONE, the response is odd
    if (addr.fromString(host) && IPAddress_isValid(addr)) {
        __LDBG_printf("resolved address %s=%s isset=%u addr=%x", host.c_str(), addr.toString().c_str(), IPAddress_isValid(addr), (uint32_t)addr);
        return true;
    }

    // hostByName can returns true and INADDR_NONE
    if (WiFi.hostByName(host.c_str(), addr) && IPAddress_isValid(addr)) {
        __LDBG_printf("resolved host %s=%s isset=%u addr=%x", host.c_str(), addr.toString().c_str(), IPAddress_isValid(addr), (uint32_t)addr);
        return true;
    }
    errorMessage.printf_P(SPGM(ping_monitor_unknown_service), host.c_str());
    return false;
}

bool PingMonitor::begin(const AsyncPingPtr &ping, const String &host, IPAddress &addr, int count, int timeout, PrintString &message)
{
    validateValues(count, timeout);
    __LDBG_printf("addr=%s count=%d timeout=%d", addr.toString().c_str(), count, timeout);
    message.printf_P(PSTR("PING %s (%s) 56(84) bytes of data."), host.c_str(), addr.toString().c_str());
    auto result = ping->begin(addr, count, timeout);
    if (!result) {
        message = F("Unknown error occured"); //ping running?
    }
    return result;
}

// ------------------------------------------------------------------------
// class PingMonitorPlugin

class PingMonitorPlugin : public PluginComponent {
public:
    PingMonitorPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    // virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;

private:
    AsyncPingPtr _ping;
#endif

private:
    friend PingMonitor::Task;

    void _setPingConsoleMenu(bool enable);
    void _setup();
    void _setupWebHandler();

    PingMonitor::TaskPtr _task;
};

static PingMonitorPlugin plugin;

bool PingMonitor::Task::hasStats()
{
    return static_cast<bool>(plugin._task);
}

PingMonitor::Statistics PingMonitor::Task::getStats()
{
    if (plugin._task) {
        return plugin._task->_stats;
    }
    return PingMonitor::Statistics();
}

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    PingMonitorPlugin,
    "pingmon",          // name
    "Ping Monitor",     // friendly name
    "",                 // web_templates
    "ping-monitor",     // config_forms
    "",             // reconfigure_dependencies
    PluginComponent::PriorityType::PING_MONITOR,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

PingMonitorPlugin::PingMonitorPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(PingMonitorPlugin))
{
    REGISTER_PLUGIN(this, "PingMonitorPlugin");
}

void PingMonitorPlugin::_setup()
{
    _ping.reset();
    _task.reset();
    _setupWebHandler();

    auto config = Plugins::Ping::getConfig();
    _setPingConsoleMenu(config.console);

    __LDBG_printf("service=%u interval=%u count=%d timeout=%d hosts=%u", config.service, config.interval, config.count, config.timeout, Plugins::Ping::getHosts().size());

    // setup ping monitor background service
    if (config.service && config.interval && config.count) {
        auto hosts = Plugins::Ping::getHosts();
        auto size = hosts.size();
        if (size) {
            _task.reset(new PingMonitor::Task(config.interval, config.count, config.timeout));
            for(uint8_t i = 0; i < size; i++) {
                _task->addHost(hosts[i]);
            }

            _task->start();
        }
    }
}

void PingMonitorPlugin::_setupWebHandler()
{
    if (wsPing) {
        __DBG_printf("wsPing=%p not null", wsPing);
        return;
    }
    if (Plugins::Ping::getConfig().console) {
        auto server = WebServer::Plugin::getWebServerObject();
        if (server) {
            auto ws = new WsClientAsyncWebSocket(F("/ping"), &wsPing);
            ws->onEvent(PingMonitor::eventHandler);
            server->addHandler(ws);
            Logger_notice("Web socket for ping running on port %u", System::WebServer::getConfig().getPort());
        }
    }
    else {
        __LDBG_print("web socket disabled");
    }
}

void PingMonitorPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _setup();
}

void PingMonitorPlugin::reconfigure(const String &source)
{
    // if (String_equals(source, SPGM(http))) {
    //     _setupWebHandler();
    // }
    // else
    {
        _setup();
    }
}

void PingMonitorPlugin::shutdown()
{
    _ping.reset();
    if (wsPing) {
        wsPing->shutdown();
    }
    _task.reset();
}

void PingMonitorPlugin::getStatus(Print &output)
{
    if (_task) {
        _task->printStats(output);
    }
    else {
        output.print(FSPGM(ping_monitor_service));
        output.print(' ');
        output.print(FSPGM(disabled));
    }
}

void PingMonitorPlugin::createMenu()
{
    bootstrapMenu.addMenuItem(getFriendlyName(), F("ping-monitor.html"), navMenu.config);
    _setPingConsoleMenu(Plugins::Ping::getConfig().console);
}

void PingMonitorPlugin::_setPingConsoleMenu(bool enable)
{
    auto label = F("Ping Remote Host");
    auto iterator = bootstrapMenu.findMenuByLabel(label, navMenu.util);
    __LDBG_printf("console=%u menu=%u", enable, bootstrapMenu.isValid(iterator));

    if (bootstrapMenu.isValid(iterator)) { // do we have a menu entry?
        if (!enable) { // console is disabled
            bootstrapMenu.remove(iterator); // remove it
        }
        return;
    }
    // add entry
    bootstrapMenu.addMenuItem(label, F("ping.html"), navMenu.util);
}

void PingMonitorPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("type=%u name=%s", type, formName.c_str());
    if (type == FormCallbackType::SAVE) {
        auto hosts = Plugins::Ping::getHosts();
        // hosts.clear();
        for (const auto &field: form.getFields()) {
            if (pgm_read_byte(field->getName()) == 'h') {
                __DBG_printf("%s=%s", field->getName(), field->getValue());
                // hosts.append(field->getValue().trim());
            }
        }
        return;
    }
    else
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Ping::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
    ui.setTitle(F("Ping Monitor Configuration"));
    ui.setContainerId(F("ping_settings"));

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("pwc"), FormGetterSetter(cfg, console));
    form.addFormUI(F("Ping Console"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("pbt"), FormGetterSetter(cfg, service));
    form.addFormUI(F("Ping Monitor Service"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("pc"), FormGetterSetter(cfg, count));
    form.addFormUI(F("Ping Count"));
    cfg.addRangeValidatorFor_count(form);

    form.addObjectGetterSetter(F("pt"), FormGetterSetter(cfg, timeout));
    form.addFormUI(FSPGM(Timeout), FormUI::Suffix(FSPGM(milliseconds)));
    cfg.addRangeValidatorFor_timeout(form);

    mainGroup.end();

    auto &serviceGroup = form.addCardGroup(F("pingbs"), FSPGM(ping_monitor_service), cfg.service);

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 8, h);
    static_assert(8 == Plugins::Ping::kHostsMax, "adjust value above");

    // if (request->methodToString() == F("POST")) {

    // }

    for(uint8_t i = 0; i < Plugins::Ping::kHostsMax; i++) {

        form.addCallbackGetterSetter<String>(F_VAR(h, i), [i](String &str, Field::BaseField &, bool store) {
            __LDBG_printf("host callback store=%u host=%s num=%u", store, str.c_str(), i);
            using Ping = Plugins::Ping;
            if (store) {
                // Ping::setHost(i, str.trim().c_str(), true);
                __DBG_printf("trimmed len=%u", str.length());
            }
            else {
                str = Plugins::Ping::getHosts()[i];
            }
            return true;
        }, InputFieldType::TEXT);

        form.addFormUI(FormUI::Label(PrintString(F("Host %u"), i + 1)));
        form.addValidator(FormUI::Validator::Length(0, Plugins::Ping::HostnameType::kMaxLength));
        form.addValidator(FormUI::Validator::HostnameEx(FormUI::Validator::Hostname::AllowedType::EMPTY)).emplace_back(FSPGM(_var_gateway, "${gateway}"));
    }

    form.addObjectGetterSetter(F("pi"), FormGetterSetter(cfg, interval));
    form.addFormUI(FSPGM(Interval), FormUI::Suffix(FSPGM(minutes)));
    cfg.addRangeValidatorFor_interval(form);

    serviceGroup.end();

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"
#include "ping_monitor.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PING, "PING", "<target[,count=4[,timeout=5000]]>", "Ping host or IP address");

ATModeCommandHelpArrayPtr PingMonitorPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(PING)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool PingMonitorPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PING))) {

        if (args.isQueryMode()) {
            args.printf_P(PSTR("ping=%p"), _ping.get());
        }
        else if (args.size() == 0) {
            if (_ping) {
                _ping.reset();
                args.print(FSPGM(ping_monitor_cancelled));
            }
        }
        else if (args.requireArgs(1, 3)) {
            auto host = args.get(0);
            IPAddress addr;
            PrintString message;

            bool error = true;
            if (_ping) {
                // remove callbacks and cancel ping to reuse object
                PingMonitor::WsPingClient::cancelPing(_ping.get());
            }
            else {
                // allocate new ping object
                _ping.reset(new AsyncPing(), PingMonitor::WsPingClient::getDefaultDeleter);
            }

            auto &serial = args.getStream();
            if (PingMonitor::resolveHost(host, addr, message)) {
                int count = args.toInt(1);
                int timeout = args.toInt(2);


                _ping->on(true, [&serial](const AsyncPingResponse &response) {
                    __LDBG_AsyncPingResponse(true, response);
                    if (response.answer) {
                        serial.printf_P(SPGM(ping_monitor_response), response.size, response.addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
                        serial.println();
                    } else {
                        serial.println(FSPGM(ping_monitor_request_timeout));
                    }
                    return false;
                });
                _ping->on(false, [this, &serial](const AsyncPingResponse &response) {
                    __LDBG_AsyncPingResponse(false, response);
                    serial.printf_P(SPGM(ping_monitor_end_response), response.addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time);
                    serial.println();
                    if (response.mac) {
                        serial.printf_P(SPGM(ping_monitor_ethernet_detected), mac2String(response.mac->addr).c_str());
                        serial.println();
                    }
                    _ping.reset();
                    return true;
                });

                error = !PingMonitor::begin(_ping, host, addr, count, timeout, message);
            }

            // message is set by PingMonitor::resolveHost or PingMonitor::begin
            serial.println(message);

            if (error) {
                _ping.reset();
            }
        }
        return true;

    }
    return false;
}

#endif
