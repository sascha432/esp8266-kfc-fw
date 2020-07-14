/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Form.h"
#include "PluginComponent.h"
#include "plugins_menu.h"

#ifndef DEBUG_ALARM_FORM
#define DEBUG_ALARM_FORM                                1
#endif

#ifndef IOT_ALARM_PLUGIN_HAS_BUZZER
#define IOT_ALARM_PLUGIN_HAS_BUZZER                     0
#endif

#ifndef IOT_ALARM_PLUGIN_HAS_SILENT
#define IOT_ALARM_PLUGIN_HAS_SILENT                     0
#endif

class AsyncWebServerRequest;

class AlarmPlugin : public PluginComponent {
public:
    AlarmPlugin();

    virtual PGM_P getName() const {
        return PSTR("alarm");
    }
    // virtual const __FlashStringHelper *getFriendlyName() const {
    //     return F("Alarm");
    // }
    virtual PriorityType getSetupPriority() const override {
        return PriorityType::ALARM;
    }

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual void configurationSaved(Form *form) override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

public:
    enum class AlarmType : uint8_t {
        SILENT,
        BUZZER,
        BOTH,
    };

private:
    void _installAlarms();
    void _removeAlarms();
};
