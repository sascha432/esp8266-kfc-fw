/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "plugins.h"
#include "plugins_menu.h"
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

class MDNSService {
public:
    static bool addService(const String &service, const String &proto, uint16_t port);
    static bool addServiceTxt(const String &service, const String &proto, const String &key, const String &value);
    static bool removeService(const String &service, const String &proto);
    static bool removeServiceTxt(const String &service, const String &proto, const String &key);
    static void announce();
};

#if MDNS_PLUGIN

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
    MDNSPlugin() : _running(false), _enabled(false)
    {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return PSTR("mdns");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("MDNS");
    }
    virtual PriorityType getSetupPriority() const override {
        return PriorityType::MDNS;
    }
    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override {
        return plugin->nameEquals(FSPGM(http));
    }
    virtual void shutdown() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

#if ESP8266

    virtual MenuType getMenuType() const override {
        return MenuType::CUSTOM;
    }
    virtual void createMenu() override {
        if (_enabled) {
            bootstrapMenu.addSubMenu(F("MDNS Discovery"), F("mdns_discovery.html"), navMenu.util);
        }
    }

    static void mdnsDiscoveryHandler(AsyncWebServerRequest *request);
    void serviceCallback(ServiceInfoVector &services, bool map, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);

#endif

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void wifiCallback(uint8_t event, void *payload);
    static void loop();

    // ESP8266: begin must be called once early in the program execution
    // otherwise it crashes when adding services
    void begin();
    void end();

private:
    friend MDNSService;
    bool _isRunning() const;

    bool _MDNS_begin();
    void _begin();
    void _end();
    void _wifiCallback(uint8_t event, void *payload);
    void _loop();
    void _installWebServerHooks();

private:
    uint8_t _running: 1;
    uint8_t _enabled: 1;
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
    EventScheduler::Timer _delayedStart;
#endif
};

#endif
