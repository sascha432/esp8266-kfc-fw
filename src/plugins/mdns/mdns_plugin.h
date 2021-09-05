/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MDNS_PLUGIN

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <build.h>
#include <kfc_fw_config.h>
#include <plugins.h>
#include <plugins_menu.h>
#include "mdns_resolver.h"
#include "mdns_sd.h"

#if ESP8266
#include <ESP8266mDNS.h>
#endif
#if ESP32
#include <ESPmDNS.h>
#endif

#ifndef DEBUG_MDNS_SD
#   define DEBUG_MDNS_SD 1
#endif

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class MDNSPlugin : public PluginComponent {
public:
    #if ESP8266
        using MDNSResponder = esp8266::MDNSImplementation::MDNSResponder;
    #elif ESP32
        using MDNSResponder = ::MDNSResponder;
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

    enum class OutputState {
        NONE,
        SERVICE,
    };

    class Output {
    public:
        String _current;
        PrintString _output;
        MDNSResolver::ServiceQuery _serviceQuery;
        uint32_t _timeout;
        SemaphoreMutex _lock;

        Output(uint32_t timeout) :
            _serviceQuery(nullptr),
            _timeout(timeout)
        {
        }

        ~Output() {
            if (_serviceQuery) {
                #if ESP8266
                    MDNS.removeServiceQuery(_serviceQuery);
                #elif ESP32
                    mdns_query_async_delete(_serviceQuery);
                #endif
            }
        }

        void begin() {
            _output.print(F("{\"l\":[{"));
        }

        void end() {
            if (_output.length()) {
                _output.trim(',');
                _output.print(F("}]}"));
                _timeout = 0;
                _current = String();
            }
        }

        void next() {
            if (_output.length()) {
                _output.trim(',');
                _output.print(F("},{"));
            }
            else {
                begin();
            }
        }

        #if ESP32
            bool poll();
        #endif
    };

public:
    MDNSPlugin();

    static bool isEnabled();
    static bool isNetBIOSEnabled();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createMenu() override {
        if (isEnabled()) {
            bootstrapMenu.addMenuItem(F("MDNS Discovery"), F("mdns-discovery.html"), navMenu.util);
        }
    }
    static void mdnsDiscoveryHandler(AsyncWebServerRequest *request);
    #if ESP8266
        void serviceCallback(Output &output, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent);
    #endif

    #if AT_MODE_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

public:
    static void loop();

    void begin();
    void end();

    static MDNSPlugin &getInstance();

public:
    void resolveZeroConf(MDNSResolver::Query *query);
    static void removeQuery(MDNSResolver::Query *query);

    MDNSResolver::Query *findQuery(void *query) const;

private:
    MDNSResolver::Queries _queries;

private:
    friend class MDNSService;

    #if MDNS_NETBIOS_SUPPORT
        void _setupNetBIOS();
    #endif
    void _startQueries();
    void _stopQueries();
    void _removeQuery(MDNSResolver::Query *query);
    bool _isRunning() const;
    void _loop();

private:
    bool _running;
};

inline void MDNSPlugin::_stopQueries()
{
    _queries.clear();
}

inline bool MDNSPlugin::isEnabled()
{
    auto flags = KFCConfigurationClasses::System::Flags::getConfig();
    return flags.is_mdns_enabled;
}

inline void MDNSPlugin::reconfigure(const String &source)
{
    __LDBG_printf("running=%u source=%s", _running, source.c_str());
    end();
    begin();
}

inline void MDNSPlugin::shutdown()
{
    __LDBG_println();
    end();
}

inline void MDNSPlugin::loop()
{
    getInstance()._loop();
}

inline bool MDNSPlugin::_isRunning() const
{
    return _running;
}

inline void MDNSPlugin::removeQuery(MDNSResolver::Query *query)
{
    getInstance()._removeQuery(query);
}

#if DEBUG_MDNS_SD
#include <debug_helper_disable.h>
#endif

#endif
