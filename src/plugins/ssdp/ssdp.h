/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_SSDP
#define DEBUG_SSDP                          0
#endif

#if IOT_SSDP_SUPPORT

#include <Arduino_compat.h>
#include <ESP8266SSDP.h>
#include <kfc_fw_config.h>
#include "plugins.h"

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

#endif
