/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

 #if IOT_SSDP_SUPPORT

#ifndef DEBUG_SSDP
#    define DEBUG_SSDP 0
#endif

#include <Arduino_compat.h>
#if ESP8266
#    include <ESP8266SSDP.h>
#elif ESP32
#    include <ESP32SSDP.h>
#endif
#include "plugins.h"
#include <kfc_fw_config.h>

#if DEBUG_SSDP
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class SSDPPlugin : public PluginComponent {
public:
    SSDPPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;

    void _begin();
    void _end();

    bool _running;
};

class SSDPClassEx : public SSDPClass
{
public:
    const char *getUUID() const {
        return _uuid;
    }
};

#if DEBUG_SSDP
#    include <debug_helper_disable.h>
#endif


 #endif
