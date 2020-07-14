/**
 * Author: sascha_lammers@gmx.de
 */


#include <PrintHtmlEntitiesString.h>
#include <ESPAsyncWebServer.h>
#include "../include/templates.h"
#include "kfc_fw_config_classes.h"
#include "plugins.h"
#include "alarm.h"

#if DEBUG_ALARM_FORM
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static AlarmPlugin plugin;

AlarmPlugin::AlarmPlugin()
{
    REGISTER_PLUGIN(this);
}

void AlarmPlugin::setup(SetupModeType mode)
{
    _installAlarms();
    _debug_println();
}

void AlarmPlugin::reconfigure(PGM_P source)
{
    _removeAlarms();
    _installAlarms();
}

void AlarmPlugin::shutdown()
{
    _removeAlarms();
}

using Alarm = KFCConfigurationClasses::Plugins::Alarm;

static uint8_t form_get_alarm_num(const String &varName)
{
#if IOT_ALARM_FORM_MAX_ALERTS <= 10
    return (varName.charAt(1) - '0');
#else
    return (uint8_t)atoi(varName.c_str() + 1);
#endif
}

static bool form_enabled_callback(bool value, FormField &field, bool store)
{
    if (store) {
        auto alarmNum = form_get_alarm_num(field.getName());
        _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
        auto &cfg = Alarm::getWriteableConfig();
        cfg.alarms[alarmNum].is_enabled = value;
    }
    return false;
}

static bool form_weekday_callback(uint8_t value, FormField &field, bool store)
{
    if (store) {
        auto alarmNum = form_get_alarm_num(field.getName());
        _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
        auto &cfg = Alarm::getWriteableConfig();
        cfg.alarms[alarmNum].time.week_day.week_days = value;
    }
    return false;
}

void AlarmPlugin::configurationSaved(Form *form)
{
    _debug_println();
    auto &cfg = Alarm::getWriteableConfig();
    Alarm::updateTimestamps(time(nullptr) + 90, cfg);
    Alarm::dump(DEBUG_OUTPUT, cfg);
}

void AlarmPlugin::getStatus(Print &output)
{
    output.print(F("Alarm Plugin"));
}

void AlarmPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    _debug_printf_P(PSTR("url=%s\n"), request->url().c_str());
    // example for memory consumption
    //
    // before optimizing: 14368
    // shortening names to less than String::SSOSIZE: -3760 byte (10608)
    //      String needs 16 byte memory if less than SSO size (ESP8266 = less than 11 characters)
    //      otherwise it allocates another 16 byte aligned block = 32 byte for strings between 11-15 characters or 48 byte for strings between 16-31 characters
    // replacing lambdas with static functions: -1200 byte (9408)
    //      lamdba functions need 16 byte (8 byte without memory alignment) or more if variables are captured
    // removing 7x checkbox and replacing it with a single field: -5056 byte (4352)
    //      requires some javascript unserialize/serialize
    //
    // the POST request consumes about the same amount of memory as the form itself
    // large forms can be split into small parts and posted via ajax one by one

    auto &cfg = Alarm::getWriteableConfig();

    for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
        String prefix = 'a' + String(i);
        auto &alarm = cfg.alarms[i];

        form.add<bool>(prefix + String('e'), alarm.is_enabled, form_enabled_callback);

        form.add<uint8_t>(prefix + String('h'), &alarm.time.hour);
        form.add<uint8_t>(prefix + String('m'), &alarm.time.minute);
#if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
        form.add<uint8_t>(prefix + String('t'), &alarm.mode);
#else
        alarm.mode_type = Alarm::AlarmModeType::BOTH;
#endif

        form.add<uint8_t>(prefix + String('w'), alarm.time.week_day.week_days, form_weekday_callback);
    }

    form.finalize();
}

void AlarmPlugin::_installAlarms()
{
    _debug_println();
}

void AlarmPlugin::_removeAlarms()
{
    _debug_println();
}
