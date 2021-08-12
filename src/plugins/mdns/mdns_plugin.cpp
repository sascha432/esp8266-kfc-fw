/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_PLUGIN

#include "mdns_sd.h"
#include <PrintString.h>
#include <EventScheduler.h>
#include <misc.h>
#include "kfc_fw_config.h"
#include "web_server.h"
#include "async_web_response.h"
#include "mdns_plugin.h"
#include "mdns_sd.h"
#if MDNS_NETBIOS_SUPPORT
#include <ESP8266NetBIOS.h>
#endif

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "build.h"

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

static MDNSPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    MDNSPlugin,
    "mdns",                 // name
    "MDNS",                 // friendly name
    "",                     // web_templates
    "",                     // config_forms
    "wifi,network",         // reconfigure_dependencies
    PluginComponent::PriorityType::MDNS,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

MDNSPlugin::MDNSPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(MDNSPlugin)),
    _running(false)
{
    REGISTER_PLUGIN(this, "MDNSPlugin");
}

void MDNSService::announce()
{
    __LDBG_printf("running=%u", MDNSPlugin::getInstance()._isRunning());
    #if ESP8266
        if (MDNSPlugin::getInstance()._isRunning()) {
            MDNS.announce();
        }
    #endif
}

#if ESP8266

void MDNSPlugin::mdnsDiscoveryHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("running=%u", plugin._isRunning());
    if (plugin._isRunning()) {
        if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
            auto timeout = request->arg(F("timeout")).toInt();
            if (timeout == 0) {
                timeout = 2000;
            }
            Output *output = new Output(millis() + timeout);
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();

            {
                auto service = String(FSPGM(kfcmdns));
                auto name = String(FSPGM(udp));
                output->_serviceQuery = MDNS.installServiceQuery(service.c_str(), name.c_str(), [output](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                    plugin.serviceCallback(*output, mdnsServiceInfo, answerType, p_bSetContent);
                });
            }
            auto response = new AsyncMDNSResponse(output);
            httpHeaders.setResponseHeaders(response);
            request->send(response);
        }
        else {
            request->send(403);
        }
    }
    else {
        request->send(500);
    }
}

#endif

#if MDNS_NETBIOS_SUPPORT

void MDNSPlugin::_setupNetBIOS()
{
    if (isNetBIOSEnabled()) {
        #if DEBUG_MDNS_SD
            auto result =
        #endif
        NBNS.begin(System::Device::getName());
        __LDBG_printf("NetBIOS result=%u", result);
    }
}

#endif

void MDNSPlugin::_startQueries()
{
    // start all queries in the queue
    __LDBG_printf("zerconf queries=%u", _queries.size());
    for(const auto &query: _queries) {
        if (query->getState() == MDNSResolver::Query::StateType::NONE) {
            query->begin();
        }
    }
}

bool MDNSPlugin::isEnabled()
{
    auto flags = System::Flags::getConfig();
    return flags.is_mdns_enabled;
}

bool MDNSPlugin::isNetBIOSEnabled()
{
    auto flags = System::Flags::getConfig();
    return flags.is_mdns_enabled && flags.is_netbios_enabled;
}

void MDNSPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    __DBG_assert_printf(mode != SetupModeType::AUTO_WAKE_UP, "not allowed SetupModeType::AUTO_WAKE_UP");;
    if (isEnabled()) {
        begin();
    }
}

MDNSPlugin &MDNSPlugin::getInstance()
{
    return plugin;
}

void MDNSPlugin::resolveZeroConf(MDNSResolver::Query *query)
{
    auto wasEmpty = _queries.empty();
    __LDBG_printf("query=%p running=%u queries=%u", query, _isRunning(), _queries.size());

    _queries.emplace_back(query);
    if (!isEnabled()) {
        //TODO this might fail if wifi is down
        if (wasEmpty) {
            __LDBG_printf("MDNS disabled, calling begin");
            MDNS.begin(System::Device::getName());
            LoopFunctions::add(loop);
        }
        query->begin();
    }
    else if (_isRunning()) {
        query->begin();
    }
}

MDNSResolver::Query *MDNSPlugin::findQuery(void *query) const
{
    auto iterator = std::find_if(_queries.begin(), _queries.end(), [query](const MDNSResolver::QueryPtr queryPtr) {
        return (queryPtr.get() == query);
    });
    if (iterator != _queries.end()) {
        return reinterpret_cast<MDNSResolver::Query *>(query);
    }
    return nullptr;
}

void MDNSPlugin::_removeQuery(MDNSResolver::Query *query)
{
    __LDBG_printf("remove=%p size=%u", query, _queries.size());
    _queries.erase(std::remove_if(_queries.begin(), _queries.end(), [query](const MDNSResolver::QueryPtr queryPtr) {
        return (queryPtr.get() == query);
    }), _queries.end());
    __LDBG_printf("size=%u", _queries.size());

    if (!isEnabled() && _queries.empty()) {
        __LDBG_printf("MDNS disabled, zeroconf done, calling end");
        MDNS.end();
        LoopFunctions::remove(loop);
    }
}

MDNSPlugin &MDNSPlugin::getPlugin()
{
    return plugin;
}

void MDNSPlugin::begin()
{
    auto hostname = System::Device::getName();
    __LDBG_printf("hostname=%s WiFi=%u running=%u", hostname, WiFi.isConnected(), _running);
    if (MDNS.begin(hostname)) {
        _running = true;
        LoopFunctions::add(loop);
        MDNSService::addService(FSPGM(kfcmdns), FSPGM(udp), 5353);
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('b'), String(__BUILD_NUMBER_INT));
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('t'), KFCConfigurationClasses::System::Device::getTitle());

        #if MDNS_NETBIOS_SUPPORT
            _setupNetBIOS();
        #endif
        _startQueries();
    }
    else {
        LoopFunctions::remove(loop);
        _running = false;
    }
}

void MDNSPlugin::end()
{
    __LDBG_printf("running=%u", _running);
    MDNS.end();
    LoopFunctions::remove(loop);
    _running = false;

    #if MDNS_NETBIOS_SUPPORT
        if (isNetBIOSEnabled()) {
            NBNS.end();
        }
    #endif
}

void MDNSPlugin::_loop()
{
    #if ESP8266
        MDNS.update();
    #endif
    if (!_queries.empty()) {
        for(auto &query: _queries) {
            query->checkTimeout();
        }
    }
}

void MDNSPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Service for hostname %s "), System::Device::getName());
    if (!_running) {
        output.print(F("not "));
    }
    output.print(F("running"));

    #if MDNS_NETBIOS_SUPPORT
        output.printf_P(PSTR(HTML_S(br) "NetBIOS %s"), isNetBIOSEnabled() ? SPGM(enabled) : SPGM(disabled));
    #endif
}

//void resolveZeroConf(const String &service, const String &proto, const String &field, const String &fallback, uint16_t port, ResolvedCallback callback);

#endif
