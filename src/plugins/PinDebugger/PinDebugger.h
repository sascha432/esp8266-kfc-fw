/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef PIN_DEBUGGER
#define PIN_DEBUGGER                            0
#endif

#if PIN_DEBUGGER

#include <Arduino_compat.h>
#include <VirtualPinDebug.h>
#include "plugins.h"

class PinDebuggerPlugin : public PluginComponent {
public:
    PinDebuggerPlugin() {
        REGISTER_PLUGIN(this);
    }
    PGM_P getName() const {
        return PSTR("pindbg");
    }

    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)-125;
    }
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(F("Ping Remote Host"), F("pin_debugger.html"), navMenu.util);
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

#endif
