/**
 * Author: sascha_lammers@gmx.de
 */

// Allows to connect over TCP to serial ports

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "plugins.h"

#ifndef DEBUG_SERIAL2TCP
#define DEBUG_SERIAL2TCP                        1
#endif

#if !SERIAL2TCP_SUPPORT
#error SERIAL2TCP_SUPPORT must be set
#endif

class Serial2TcpPlugin : public PluginComponent {
public:
    using Serial2TCP = KFCConfigurationClasses::Plugins::Serial2TCP;

public:
    Serial2TcpPlugin();

    virtual PGM_P getName() const {
        return PSTR("serial2tcp");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Serial2TCP");
    }
    virtual PriorityType getSetupPriority() const override {
        return PriorityType::SERIAL2TCP;
    }

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual PGM_P getConfigureForm() const {
        return PSTR("serial2tcp");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const {
        return true;
    }
    virtual void atModeHelpGenerator();
    virtual bool atModeHandler(AtModeArgs &args);
#endif
};
