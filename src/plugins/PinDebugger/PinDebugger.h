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
        REGISTER_PLUGIN(this, "PinDebuggerPlugin");
    }
    PGM_P getName() const {
        return PSTR("pindbg");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("PIN Debugger");
    }
    virtual PriorityType getSetupPriority() const override {
        return (PriorityType)-125;
    }
    virtual OptionsType getOptions() const override {
        return EnumHelper::Bitset::all(OptionsType::HAS_STATUS, OptionsType::HAS_CUSTOM_MENU, OptionsType::HAS_AT_MODE);
    }

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;

    virtual void createMenu() override {
        bootstrapMenu.addMenuItem(getFriendlyName(), F("pin-debugger.html"), navMenu.util);
    }

    // virtual PGM_P getConfigureForm() const override {
    //     return PSTR("ping_monitor");
    // }
    // virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};
