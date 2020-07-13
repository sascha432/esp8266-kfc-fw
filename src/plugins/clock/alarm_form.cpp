/**
 * Author: sascha_lammers@gmx.de
 */


#include <PrintHtmlEntitiesString.h>
#include <ESPAsyncWebServer.h>
#include "../include/templates.h"
#include "plugins.h"
#include "alarm_form.h"
#include "kfc_fw_config_classes.h"

#if DEBUG_ALARM_FORM
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void AlarmForm::setup(PluginSetupMode_t mode)
{
    _debug_println();
}

void AlarmForm::reconfigure(PGM_P source)
{
    _debug_println();
}

using Alarm = KFCConfigurationClasses::Plugins::Alarm;

static bool form_enabled_callback(bool value, FormField &field, bool store)
{
    uint8_t alarmNum = (field.getName().charAt(1) - '0');
    _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
    if (store) {
        auto &cfg = Alarm::getWriteableConfig();
        Alarm::setEnabled(cfg.alarms[alarmNum], value);
    }
    return false;
}

static bool form_weekday_callback(bool value, FormField &field, bool store)
{
    uint8_t alarmNum = (field.getName().charAt(1) - '0');
    uint8_t weekdayNum = (field.getName().charAt(3) - '0');
    _debug_printf_P(PSTR("name=%s alarm=%u weekday=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, weekdayNum, store, value);
    if (store) {
        auto &cfg = Alarm::getWriteableConfig();
        auto &weekdays = cfg.alarms[alarmNum].time.week_day.week_days;
        if (value) {
            weekdays |= _BV(weekdayNum);
        }
        else {
            weekdays &= ~_BV(weekdayNum);
        }
    }
    return false;
}

void AlarmForm::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
// TODO runs out of memory
// 14368 !!!
// short names: 10608
// no lambdas 9408

    size_t heap = ESP.getFreeHeap();
    auto &cfg = Alarm::getWriteableConfig();

    for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
        String prefix = 'a' + String(i);
        auto &alarm = cfg.alarms[i];

        form.add<bool>(prefix + String('e'), Alarm::isEnabled(alarm), form_enabled_callback);

        form.add<uint8_t>(prefix + String('h'), &alarm.time.hour);
        form.add<uint8_t>(prefix + String('m'), &alarm.time.minute);
        form.add<uint8_t>(prefix + String('t'), &alarm.mode);

        for(uint8_t j = 0; j < 7; j++) {
            form.add<bool>(prefix + String('w') + String(j), alarm.time.week_day.week_days & _BV(j), form_weekday_callback, FormField::InputFieldType::CHECK);
        }
    }

    form.finalize();
    _debug_printf_P(PSTR("heap %u\n"), heap-ESP.getFreeHeap());
}
