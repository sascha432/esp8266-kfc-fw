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
#    include <ESP8266NetBIOS.h>
#endif

// TODO
// change async polling to notifier callback (currently missing in mdns_query_async_new, probably needs an upgrade of the framework)
// crashes with nullptr exception inside idf framework (bug is fixed in a recent version of the framework)
// servicedomain missing, using instance name instead. querying the servicedomain crashes with nullptr exception

#if DEBUG_MDNS_SD
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#include "build.h"

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

MDNSPlugin mDNSPlugin;

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

void MDNSPlugin::mdnsDiscoveryHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("running=%u", getInstance()._isRunning());
    if (getInstance()._isRunning()) {
        if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
            auto timeout = request->arg(F("timeout")).toInt();
            if (timeout == 0) {
                timeout = 3000;
            }
            auto output = new Output(timeout);
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            {
                #if ESP8266
                    auto service = String(FSPGM(kfcmdns));
                    auto name = String(FSPGM(udp));
                    output->_serviceQuery = MDNS.installServiceQuery(service.c_str(), name.c_str(), [output](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                        getInstance().serviceCallback(*output, mdnsServiceInfo, answerType, p_bSetContent);
                    });
                #elif ESP32
                    __LDBG_printf("mdns_query_async_new service=%s proto=%s timeout=%u", PSTR("_kfcmdns"), PSTR("_udp"), timeout);
                    output->_serviceQuery = mdns_query_async_new(nullptr, PSTR("_kfcmdns"), PSTR("_udp"), MDNS_TYPE_PTR, timeout, 64, NULL);
                    if (!output->_serviceQuery) {
                        __LDBG_printf("mdns_query_async_new failed");
                    }
                #endif
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
    __LDBG_printf("zeroconf queries=%u", _queries.size());
    MUTEX_LOCK_BLOCK(_lock) {
        // start all queries in the queue
        for(const auto &query: _queries) {
            if (query->getState() == MDNSResolver::Query::StateType::NONE) {
                query->begin();
            }
        }
    }
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

void MDNSPlugin::resolveZeroConf(MDNSResolver::Query *query)
{
    bool wasEmpty = false;
    MUTEX_LOCK_BLOCK(_lock) {
        wasEmpty = _queries.empty();
        __LDBG_printf("query=%p running=%u queries=%u", query, _isRunning(), _queries.size());
        _queries.emplace_back(query);
    }
    if (!isEnabled()) {
        //TODO this might fail if wifi is down
        if (wasEmpty) {
            __LDBG_printf("MDNS disabled, calling begin");
            MDNS.begin(System::Device::getName());
            LOOP_FUNCTION_ADD(loop);
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
    bool empty = true;
    MUTEX_LOCK_BLOCK(_lock) {
        __LDBG_printf("remove=%p size=%u", query, _queries.size());
        _queries.erase(std::remove_if(_queries.begin(), _queries.end(), [query](const MDNSResolver::QueryPtr queryPtr) {
            return (queryPtr.get() == query);
        }), _queries.end());
        __LDBG_printf("size=%u", _queries.size());
        empty = _queries.empty();
    }

    if (!isEnabled() && empty) {
        __LDBG_printf("MDNS disabled, zeroconf done, calling end");
        MDNS.end();
        LoopFunctions::remove(loop);
    }
}

void MDNSPlugin::begin()
{
    auto hostname = System::Device::getName();
    __LDBG_printf("hostname=%s WiFi=%u running=%u", hostname, WiFi.isConnected(), _running);
    if (MDNS.begin(hostname)) {
        _running = true;
        LOOP_FUNCTION_ADD(loop);
        MDNSService::addService(FSPGM(kfcmdns), FSPGM(udp), 5353);
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('b'), String(__BUILD_NUMBER_INT));
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('t'), KFCConfigurationClasses::System::Device::getTitle());
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('d'), KFCFWConfiguration::getChipModel());

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
    _stopQueries();
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
        __LDBG_printf("queries=%u", _queries.size());
        MUTEX_LOCK_BLOCK(_lock) {
            for(auto &query: _queries) {
                query->check();
            }
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

#endif
