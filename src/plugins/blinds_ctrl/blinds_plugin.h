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

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void loopMethod();

    static BlindsControlPlugin &getInstance();

#if IOT_BLINDS_CTRL_RPM_PIN
    static void rpmIntCallback(InterruptInfo info);
#endif
};

inline __attribute__((__always_inline__))
void BlindsControlPlugin::getValues(WebUINS::Events &array)
{
    BlindsControl::getValues(array);
}

inline __attribute__((__always_inline__))
void BlindsControlPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    BlindsControl::setValue(id, value, hasValue, state, hasState);
}

inline __attribute__((__always_inline__))
void BlindsControlPlugin::loopMethod()
{
    getInstance()._loopMethod();
}

// ------------------------------------------------------------------------
// BlindsControl
// ------------------------------------------------------------------------

inline void BlindsControl:: startToneTimer(uint32_t timeout)
{
    __LDBG_printf("timeout=%u", timeout);
    BlindsControlPlugin::getInstance()._startToneTimer(timeout);
}

#if HAVE_IMPERIAL_MARCH

inline void BlindsControl::playImperialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat)
{
    BlindsControlPlugin::getInstance()._playImperialMarch(speed, zweiklang, repeat);
}

#endif

inline void BlindsControl::stopToneTimer(ActionStateType state)
{
    __LDBG_printf("state=%u", state);
    if (state != ActionStateType::DELAY) {
        BlindsControlPlugin::getInstance()._stopToneTimer();
    }
}
