/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "ntp_plugin.h"
#include "plugins.h"

class AtModeArgs;
class AsyncWebServerRequest;

class NTPPlugin : public PluginComponent {
public:
    NTPPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    // sets the callback and executes configTime()
    static void execConfigTime();
    static void updateNtpCallback();
    static void _checkTimerCallback(Event::CallbackTimerPtr timer);

public:
    static uint32_t _ntpRefreshTimeMillis;

private:
    Event::Timer _checkTimer;
};
