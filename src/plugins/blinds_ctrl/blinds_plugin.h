/**
 * Author: sascha_lammers@gmx.de
 */

// controller
// https://easyeda.com/sascha23095123423/iot_blinds_controller

// optional position tracking/rpm sensing/improved stall detection
// https://easyeda.com/sascha23095123423/rpm-sensing-for-iot-blinds-controller

#pragma once

#include <Arduino_compat.h>
#include <at_mode.h>
#include <plugins.h>
#include <PluginComponent.h>
#include "blinds_defines.h"
#include "BlindsControl.h"

class AsyncWebServerRequest;

class BlindsControlPlugin : public PluginComponent, public BlindsControl {
public:
    BlindsControlPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUI &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

#if AT_MODE_SUPPORTED && 0
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void loopMethod();

#if IOT_BLINDS_CTRL_RPM_PIN
    static void rpmIntCallback(InterruptInfo info);
#endif
};
