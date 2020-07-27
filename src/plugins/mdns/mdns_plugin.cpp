/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_PLUGIN

#include "mdns_sd.h"
#include <PrintString.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <LoopFunctions.h>
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

using Flags = KFCConfigurationClasses::System::Flags;

static MDNSPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    MDNSPlugin,
    "mdns",                 // name
    "MDNS",                 // friendly name
    "",                     // web_templates
    "",                     // config_forms
    "wifi,network,http",    // reconfigure_dependencies
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

MDNSPlugin::MDNSPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(MDNSPlugin))
{
    REGISTER_PLUGIN(this, "MDNSPlugin");
}

void MDNSService::announce()
{
    __LDBG_printf("running=%u", plugin._isRunning());
#if ESP8266
    if (plugin._isRunning()) {
        MDNS.announce();
    }
#endif
}

void MDNSPlugin::_installWebServerHooks()
{
#if ESP8266
    auto server = WebServerPlugin::getWebServerObject();
    if (server) {
        __LDBG_println();
        WebServerPlugin::addHandler(F("/mdns_discovery"), mdnsDiscoveryHandler);
    }
#endif
}

#if ESP8266

void MDNSPlugin::mdnsDiscoveryHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("running=%u", plugin._isRunning());
    if (plugin._isRunning()) {
        if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
            auto timeout = request->arg(F("timeout")).toInt();
            if (timeout == 0) {
                timeout = 2000;
            }
            ServiceInfoVector *services = new ServiceInfoVector();
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();

            auto serviceQuery = MDNS.installServiceQuery(String(FSPGM(kfcmdns)).c_str(), String(FSPGM(udp)).c_str(), [services](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                plugin.serviceCallback(*services, true, mdnsServiceInfo, answerType, p_bSetContent);
            });
            auto response = new AsyncMDNSResponse(serviceQuery, services, timeout);
            httpHeaders.setAsyncBaseResponseHeaders(response);
            request->send(response);
        } else {
            request->send(403);
        }
    } else {
        request->send(500);
    }
}

#endif

void MDNSPlugin::_wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    __LDBG_printf("event=%u, running=%u", event, _running);
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        _end();
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
        _delayedStart.add(MDNS_DELAYED_START_AFTER_WIFI_CONNECT, false, [this](EventScheduler::TimerPtr) {
            _begin();
        });
#else
        _begin();
#endif
#if MDNS_NETBIOS_SUPPORT
#if DEBUG_MDNS_SD
        auto result =
#endif
        NBNS.begin(config.getDeviceName());
        __LDBG_printf("NetBIOS result=%u", result);
#endif

        // start all queries in the queue
        for(const auto &query: _queries) {
            if (query->getState() == MDNSResolver::Query::StateType::NONE) {
               query->begin();
            }
        }

    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
#if MDNS_NETBIOS_SUPPORT
        NBNS.end();
#endif
        _end();
    }
}

void MDNSPlugin::setup(SetupModeType mode)
{
    auto flags = Flags(true);
    if (flags.isMDNSEnabled() && flags.isStationEnabled()) {
        _enabled = true;
        begin();

        dependsOn(FSPGM(http), [this](const PluginComponent *pluginPtr) {
            _running = false;
            // add wifi handler after all plugins have been initialized
            WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
            if (config.isWiFiUp()) {
                plugin._wifiCallback(WiFiCallbacks::EventType::CONNECTED, nullptr); // simulate event if already connected
            }
            _installWebServerHooks();
        });
    }
}

void MDNSPlugin::reconfigure(const String &source)
{
    __LDBG_printf("running=%u source=%s", _running, source.c_str());
    if (String_equals(source, SPGM(http))) {
        _installWebServerHooks();
    }
    else {
        _end();
        _begin();
    }
}

void MDNSPlugin::shutdown()
{
    __LDBG_println();
    end();
}

void MDNSPlugin::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    plugin._wifiCallback(event, payload);
}

void MDNSPlugin::loop()
{
    plugin._loop();
}

void MDNSPlugin::begin()
{
    __LDBG_println();

    if (_MDNS_begin()) {
        _running = true;
        if (MDNSService::addService(FSPGM(kfcmdns), FSPGM(udp), 5353)) {
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('b'), F(__BUILD_NUMBER));
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('t'), config._H_STR(Config().device_title));
        }
    }
}

void MDNSPlugin::end()
{
    __LDBG_printf("running=%u", _running);
    if (_running) {
        MDNSService::removeService(FSPGM(kfcmdns), FSPGM(udp));
        MDNSService::announce();
        _end();
    }
}

void MDNSPlugin::resolveZeroConf(MDNSResolver::Query *query)
{
    auto wasEmpty = _queries.empty();
    __LDBG_printf("query=%p running=%u queries=%u", query, _isRunning(), _queries.size());

    _queries.emplace_back(query);
    if (!Flags(true).isMDNSEnabled() && wasEmpty) {
        __LDBG_printf("MDNS disabled, calling begin");
        MDNS.begin(config.getDeviceName());
        query->begin();
        LoopFunctions::add(loop);
    }
    else if (_isRunning()) {
        query->begin();
    }
}

void MDNSPlugin::removeQuery(MDNSResolver::Query *query)
{
    plugin._removeQuery(query);
}

void MDNSPlugin::_removeQuery(MDNSResolver::Query *query)
{
    __LDBG_printf("remove=%p size=%u", query, _queries.size());
    _queries.erase(std::remove_if(_queries.begin(), _queries.end(), [query](const MDNSResolver::QueryPtr queryPtr) {
        return (queryPtr.get() == query);
    }), _queries.end());
    __LDBG_printf("size=%u", _queries.size());

    if (!Flags(true).isMDNSEnabled() && _queries.empty()) {
        __LDBG_printf("MDNS disabled, calling end");
        MDNS.end();
        LoopFunctions::remove(loop);
    }
}

bool MDNSPlugin::_isRunning() const
{
    return _running;
}

bool MDNSPlugin::_MDNS_begin()
{
#if ESP8266
    auto address = config.isWiFiUp() ? WiFi.localIP() : INADDR_ANY;
    __LDBG_printf("hostname=%s address=%s", config.getDeviceName(), address.toString().c_str());
    auto result = MDNS.begin(config.getDeviceName(), address);
    __LDBG_printf("result=%u", result);
    return result;
#else
    __LDBG_printf("hostname=%s", config.getDeviceName());
    return _debug_print_result(MDNS.begin(config.getDeviceName()));
#endif
}

MDNSPlugin &MDNSPlugin::getPlugin()
{
    return plugin;
}

void MDNSPlugin::_begin()
{
    __LDBG_printf("running=%u", _running);
    if (_MDNS_begin()) {
        _running = true;
        LoopFunctions::add(loop);
        MDNSService::announce();
    }
    else {
        _end();
    }
}

void MDNSPlugin::_end()
{
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
    _delayedStart.remove();
#endif
    __LDBG_printf("running=%u", _running);
    __LDBG_println();
    MDNS.end();
    _running = false;
    LoopFunctions::remove(loop);
}

void MDNSPlugin::_loop()
{
// #if DEBUG_MDNS_SD
//     static unsigned long _update_timer = 0;
//     if (millis() < _update_timer) {
//         return;
//     }
//     _update_timer = millis() + 1000;
//     _debug_println(F("MDNS.update()"));
// #endif
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
    output.printf_P(PSTR("Service for hostname %s "), config.getDeviceName());
    if (!_running) {
        output.print(F("not "));
    }
    output.print(F("running"));
}

//void resolveZeroConf(const String &service, const String &proto, const String &field, const String &fallback, uint16_t port, ResolvedCallback callback);

#endif
