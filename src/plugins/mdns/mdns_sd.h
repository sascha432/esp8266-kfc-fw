/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MDNS_SD
#define DEBUG_MDNS_SD                   0
#endif

#include <Arduino_compat.h>
#include "progmem_data.h"
#include "plugins.h"
#include "plugins_menu.h"
#if defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#if defined(ESP32)
#include <ESPmDNS.h>
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
    MDNSPlugin() : _running(false) {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return PSTR("mdns");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("MDNS");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_MDNS;
    }
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override {
        return plugin->nameEquals(FSPGM(http));
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

#if ESP8266

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(F("MDNS Discovery"), F("mdns_discovery.html"), navMenu.util);
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
    static void loop();
    void start(const IPAddress &address);
    void stop();

private:
    void _loop();
    void _installWebServerHooks();

private:
    bool _running;
};

#endif
