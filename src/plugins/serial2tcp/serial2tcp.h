/**
 * Author: sascha_lammers@gmx.de
 */

// Allows to connect over TCP to serial ports

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "plugins.h"

#ifndef DEBUG_SERIAL2TCP
#    define DEBUG_SERIAL2TCP 0
#endif

#if !SERIAL2TCP_SUPPORT
#    error SERIAL2TCP_SUPPORT must be set
#endif

class Serial2TcpPlugin : public PluginComponent
{
public:
    using Serial2TCP = KFCConfigurationClasses::Plugins::Serial2TCP;

public:
    Serial2TcpPlugin();
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    #if AT_MODE_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif
};
