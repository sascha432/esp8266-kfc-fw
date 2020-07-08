/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <VirtualPinDebug.h>
#include "plugins.h"
#include "plugins_menu.h"

class PinDebuggerPlugin : public PluginComponent {
public:
    PinDebuggerPlugin() {
        REGISTER_PLUGIN(this);
    }
    PGM_P getName() const {
        return PSTR("pindbg");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("PIN Debugger");
    }

    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)-125;
    }
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(getFriendlyName(), F("pin_debugger.html"), navMenu.util);
    }

    // virtual PGM_P getConfigureForm() const override {
    //     return PSTR("ping_monitor");
    // }
    // virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};
