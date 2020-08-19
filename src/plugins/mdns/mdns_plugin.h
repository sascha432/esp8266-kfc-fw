/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MDNS_PLUGIN

#include <Arduino_compat.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include "kfc_fw_config.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "mdns_resolver.h"
#if defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#if defined(ESP32)
#include <ESPmDNS.h>
#endif

#ifndef DEBUG_MDNS_SD
#define DEBUG_MDNS_SD                                       0
#endif

#ifndef MDNS_DELAYED_START_AFTER_WIFI_CONNECT
// #define MDNS_DELAYED_START_AFTER_WIFI_CONNECT               10000UL
#define MDNS_DELAYED_START_AFTER_WIFI_CONNECT               0
#endif

class MDNSPlugin : public PluginComponent {
#if ESP8266
public:
    using MDNSResponder = esp8266::MDNSImplementation::MDNSResponder;

#endif

    class ServiceInfo {
    public:
        ServiceInfo(const char *_serviceDomain) : serviceDomain(_serviceDomain), port(0) {
        }
        String serviceDomain;
        String domain;
        uint16_t port;
        std::vector<IPAddress> addresses;
        String txts;
        std::map<const String, String> map;
    };
    typedef std::vector<ServiceInfo> ServiceInfoVector;

public:
    MDNSPlugin();

    static bool isEnabled();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

#if ESP8266
    virtual void createMenu() override {
        if (_enabled) {
            bootstrapMenu.addSubMenu(F("MDNS Discovery"), F("mdns_discovery.html"), navMenu.util);
        }
    }
    static void mdnsDiscoveryHandler(AsyncWebServerRequest *request);
    void serviceCallback(ServiceInfoVector &services, bool map, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);

#endif

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);
    static void loop();

    // ESP8266: begin must be called once early in the program execution
    // otherwise it crashes when adding services
    void begin();
    void end();

public:
    void resolveZeroConf(MDNSResolver::Query *query);
    static void removeQuery(MDNSResolver::Query *query);

    static MDNSPlugin &getPlugin();
    MDNSResolver::Query *findQuery(void *query) const;

private:
    MDNSResolver::Queries _queries;

private:
    friend class MDNSService;

    void _removeQuery(MDNSResolver::Query *query);

    bool _isRunning() const;

    bool _MDNS_begin();
    void _begin();
    void _end();
    void _wifiCallback(WiFiCallbacks::EventType event, void *payload);
    void _loop();
    void _installWebServerHooks();

private:
    uint8_t _running: 1;
    uint8_t _enabled: 1;
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
    Event::Timer _delayedStart;
#endif
};

#endif
