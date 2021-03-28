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

#if DEBUG_PING_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

FLASH_STRING_GENERATOR_AUTO_INIT(
    AUTO_STRING_DEF(ping_monitor_response, "%d bytes from %s: icmp_seq=%d ttl=%d time=%ld ms")
    AUTO_STRING_DEF(ping_monitor_end_response, "Total answer from %s sent %d recevied %d time %ld ms")
    AUTO_STRING_DEF(ping_monitor_ethernet_detected, "Detected eth address %s")
    AUTO_STRING_DEF(ping_monitor_request_timeout, "Request timed out.")
    AUTO_STRING_DEF(ping_monitor_service_status, "Ping monitor service has been %s")
    AUTO_STRING_DEF(ping_monitor_ping_for_hostname_failed, "Pinging %s failed")
    AUTO_STRING_DEF(ping_monitor_cancelled, "Ping cancelled")
    AUTO_STRING_DEF(ping_monitor_service, "Ping Monitor Service")
);

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

// safely cancel ping and remove callbacks
void _pingMonitorSafeCancelPing(AsyncPingPtr &ping)
{
    __LDBG_printf("ping=%p", ping.get());
    if (ping) {
#if DEBUG_PING_MONITOR
        ping->on(true, [](const AsyncPingResponse &response) {
            __LDBG_print("on=received callback removed");
            return true;
        });
        ping->on(false, [](const AsyncPingResponse &response) {
            __LDBG_print("on=sent callback removed");
            return false;
        });
#else
        ping->on(true, nullptr);
        ping->on(false, nullptr);
#endif
        ping->cancel();
    }
}

void _pingMonitorSafeDeletePing(AsyncPingPtr &ping)
{
    __LDBG_printf("ping=%p", ping.get());
    if (ping) {
        _pingMonitorSafeCancelPing(ping);
        ping.reset();
    }
}

void _pingMonitorResolveHostVariables(String &host)
{
    host.replace(FSPGM(_var_gateway), WiFi.isConnected() ? WiFi.gatewayIP().toString() : emptyString);
    host.trim();
}

bool _pingMonitorResolveHost(const String &host, IPAddress &addr, PrintString &errorMessage)
{
    if (!host.length()) {
        errorMessage.print(FSPGM(ping_monitor_cancelled));
        return false;
    }

    // do not allow INADDR_NONE, the response is odd
    if (addr.fromString(host) && addr != INADDR_NONE && addr.isSet()) {
        __LDBG_printf("resolved address %s=%s isset=%u addr=%x", host.c_str(), addr.toString().c_str(), addr.isSet(), (uint32_t)addr);
        return true;
    }

    // hostByName can returns true and INADDR_NONE
    if (WiFi.hostByName(host.c_str(), addr) && addr != INADDR_NONE && addr.isSet()) {
        __LDBG_printf("resolved host %s=%s isset=%u addr=%x", host.c_str(), addr.toString().c_str(), addr.isSet(), (uint32_t)addr);
        return true;
    }
    errorMessage.printf_P(SPGM(ping_monitor_unknown_service), host.c_str());
    return false;
}

void _pingMonitorValidateValues(int &count, int &timeout)
{
    if (count < Plugins::PingConfig::PingConfig_t::kRepeatCountMin) {
        count = Plugins::PingConfig::PingConfig_t::kRepeatCountDefault;
    }
    count = std::min((int)Plugins::PingConfig::PingConfig_t::kRepeatCountMax, count);
    if (timeout < Plugins::PingConfig::PingConfig_t::kTimeoutMin) {
        timeout = Plugins::PingConfig::PingConfig_t::kTimeoutDefault;
    }
    timeout = std::min((int)Plugins::PingConfig::PingConfig_t::kTimeoutMax, timeout);
}

bool _pingMonitorBegin(const AsyncPingPtr &ping, const String &host, IPAddress &addr, int count, int timeout, PrintString &message)
{
    _pingMonitorValidateValues(count, timeout);
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
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;

private:
    AsyncPingPtr _ping;
#endif

private:
    friend PingMonitorTask;

    void _setPingConsoleMenu(bool enable);
    void _setup();
    void _setupWebHandler();

    PingMonitorTask::PingMonitorTaskPtr _task;
};

static PingMonitorPlugin plugin;

bool PingMonitorTask::hasStats()
{
    if (!plugin._task) {
        return false;
    }
    return plugin._task->_stats.hasData();
}

PingMonitorTask::PingStatistics PingMonitorTask::getStats()
{
    if (plugin._task) {
        return plugin._task->_stats;
    }
    return PingStatistics();
}

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    PingMonitorPlugin,
    "pingmon",          // name
    "Ping Monitor",     // friendly name
    "",                 // web_templates
    "ping_monitor",     // config_forms
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
    _pingMonitorSafeDeletePing(_ping);
    _task.reset();
    _setupWebHandler();

    auto config = Plugins::Ping::getConfig();
    _setPingConsoleMenu(config.console);

    __LDBG_printf("service=%u interval=%u count=%d timeout=%d hosts=%u", config.service, config.interval, config.count, config.timeout, Plugins::Ping::getHostCount());

    // setup ping monitor background service
    if (config.service && config.interval && config.count && Plugins::Ping::getHostCount()) {

        _task.reset(new PingMonitorTask(config.interval, config.count, config.timeout));
        for(uint8_t i = 0; i < Plugins::Ping::kHostsMax; i++) {
            _task->addHost(Plugins::Ping::getHost(i));
        }

        _task->start();
    }
}

void PingMonitorPlugin::_setupWebHandler()
{
    _pingMonitorSetupWebHandler();
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
    _pingMonitorSafeDeletePing(_ping);
    _pingMonitorShutdownWebSocket();
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
        Plugins::Ping::removeEmptyHosts();
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Ping::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
    ui.setTitle(F("Ping Monitor Configuration"));
    ui.setContainerId(F("ping_settings"));

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(F("pwc"), cfg, Plugins::PingConfig::PingConfig_t::get_bits_console, Plugins::PingConfig::PingConfig_t::set_bits_console);
    form.addFormUI(F("Ping Console"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("pbt"), cfg, Plugins::PingConfig::PingConfig_t::get_bits_service, Plugins::PingConfig::PingConfig_t::set_bits_service);
    form.addFormUI(F("Ping Monitor Service"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("pc"), cfg, Plugins::PingConfig::PingConfig_t::get_bits_count, Plugins::PingConfig::PingConfig_t::set_bits_count);
    form.addFormUI(F("Ping Count"));
    form.addValidator(FormUI::Validator::Range(Plugins::PingConfig::PingConfig_t::kRepeatCountMin, Plugins::PingConfig::PingConfig_t::kRepeatCountMax));

    form.add(F("pt"), _H_W_STRUCT_VALUE(cfg, timeout));
    form.addFormUI(FSPGM(Timeout), FormUI::Suffix(FSPGM(milliseconds)));
    form.addValidator(FormUI::Validator::Range(Plugins::PingConfig::PingConfig_t::kTimeoutMin, Plugins::PingConfig::PingConfig_t::kTimeoutMax));

    mainGroup.end();

    auto &serviceGroup = form.addCardGroup(F("pingbs"), FSPGM(ping_monitor_service), cfg.service);

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 8, h);
    static_assert(8 == Plugins::Ping::kHostsMax, "change value");

    for(uint8_t i = 0; i < Plugins::Ping::kHostsMax; i++) {

        form.addCallbackGetterSetter<String>(F_VAR(h, i), [i](String &str, Field::BaseField &, bool store) {
            if (store) {
                Plugins::Ping::setHost(i, str.c_str());
            } else {
                str = Plugins::Ping::getHost(i);
            }
            return true;
        }, InputFieldType::TEXT);

        //form.addStringGetterSetter(FPSTR(hostNames[i]), [i]() { return Plugins::Ping::getHost(i); }, [i](const char *hostname) { Plugins::Ping::setHost(i, hostname); });

        form.addFormUI(FormUI::Label(PrintString(F("Host %u"), i + 1)));
        form.addValidator(FormUI::Validator::HostnameEx(FormUI::Validator::Hostname::AllowedType::EMPTY)).emplace_back(FSPGM(_var_gateway, "${gateway}"));
        Plugins::Ping::addHost1LengthValidator(form); // length is the same for all

    }

    //form.addReference(F("pi"), cfg.interval);
    form.add(F("pi"), _H_W_STRUCT_VALUE(cfg, interval));
    form.addFormUI(FSPGM(Interval), FormUI::Suffix(FSPGM(minutes)));
    form.addValidator(FormUI::Validator::Range(Plugins::PingConfig::PingConfig_t::kIntervalMin, Plugins::PingConfig::PingConfig_t::kIntervalMax));

    serviceGroup.end();

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(PING, "PING", "<target[,count=4[,timeout=5000]]>", "Ping host or IP address");

void PingMonitorPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(PING), getName_P());
}

bool PingMonitorPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(PING))) {

        if (args.isQueryMode()) {
            args.printf_P(PSTR("ping=%p"), _ping.get());
        }
        else if (args.size() == 0) {
            if (_ping) {
                _pingMonitorSafeDeletePing(_ping);
                args.print(FSPGM(ping_monitor_cancelled));
            }
        }
        else if (args.requireArgs(1, 3)) {
            auto host = args.get(0);
            IPAddress addr;
            PrintString message;

            bool error = true;
            if (_ping) {
                _pingMonitorSafeCancelPing(_ping);
            }
            else {
                _ping.reset(new AsyncPing());
            }

            auto &serial = args.getStream();
            if (_pingMonitorResolveHost(host, addr, message)) {
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
                    _pingMonitorSafeDeletePing(_ping);
                    return true;
                });

                error = !_pingMonitorBegin(_ping, host, addr, count, timeout, message);
            }

            // message is set by _pingMonitorResolveHost or _pingMonitorBegin
            serial.println(message);

            if (error) {
                _pingMonitorSafeDeletePing(_ping);
            }
        }
        return true;

    }
    return false;
}

#endif
