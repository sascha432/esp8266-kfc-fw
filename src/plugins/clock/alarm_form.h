/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Form.h"
#include "PluginComponent.h"
#include "plugins_menu.h"

#ifndef DEBUG_ALARM_FORM
#define DEBUG_ALARM_FORM                            1
#endif

#ifndef IOT_ALARM_FORM_HAS_BUZZER
#define IOT_ALARM_FORM_HAS_BUZZER                   1
#endif

#ifndef IOT_ALARM_FORM_HAS_SILENT
#define IOT_ALARM_FORM_HAS_SILENT                   1
#endif

#if IOT_ALARM_FORM_ENABLED
using AlarmFormClass = class AlarmForm;
#else
using AlarmFormClass = PluginComponent;
#endif

class AsyncWebServerRequest;

class AlarmForm : public PluginComponent {
public:
    virtual bool canHandleForm(const String &formName) const override {
        debug_printf_P(PSTR("name=%s\n"), formName.c_str());
        return String_equals(formName, PSTR("alarm"));
    }

    virtual MenuTypeEnum_t getMenuType() const {
        return MenuTypeEnum_t::CUSTOM;
    }

    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(F("Alarm"), F("alarm.html"), navMenu.config);
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

public:
    enum class AlarmType : uint8_t {
        SILENT,
        BUZZER,
        BOTH,
    };

    static constexpr uint16_t UNLIMITED = ~0;

    // trigger alarm of type for duration in seconds or UNLIMITED
    virtual void triggerAlarm(AlarmType type, uint16_t duration) = 0;
};
