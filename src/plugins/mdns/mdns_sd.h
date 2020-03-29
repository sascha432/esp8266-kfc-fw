/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MDNS_SD
#define DEBUG_MDNS_SD                   0
#endif

#include <Arduino_compat.h>
#include "plugins.h"
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

public:
    MDNSPlugin() : _running(false) {
        REGISTER_PLUGIN(this);
    }
    PGM_P getName() const {
        return PSTR("mdns");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("MDNS");
    }
    PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_MDNS;
    }
    void setup(PluginSetupMode_t mode) override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

#if AT_MODE_SUPPORTED
    class ServiceInfo {
    public:
        ServiceInfo(const char *_serviceDomain) : serviceDomain(_serviceDomain), port(0) {
        }
        String serviceDomain;
        String domain;
        uint16_t port;
        std::vector<IPAddress> addresses;
        String txts;
    };
    typedef std::vector<ServiceInfo> ServiceInfoVector;

    bool hasAtMode() const override {
        return true;
    }
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
    void serviceCallback(MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);

private:
    ServiceInfoVector _services;
#endif

public:
    static void loop();
    void start(const IPAddress &address);
    void stop();

private:
    void _loop();

private:
    bool _running;
};

#endif
