/**
 * Author: sascha_lammers@gmx.de
 */


#include <PrintHtmlEntitiesString.h>
#include <ESPAsyncWebServer.h>
#include <EventTimer.h>
#include <LoopFunctions.h>
#include "../include/templates.h"
#include "../src/plugins/ntp/ntp_plugin.h"
#include "plugins.h"
#include "alarm.h"

#if DEBUG_ALARM_FORM
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static AlarmPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    AlarmPlugin,
    "alarm",            // name
    "Alarm",            // friendly name
    "",                 // web_templates
    "alarm",            // config_forms
    "mqtt",             // reconfigure_dependencies
    PluginComponent::PriorityType::ALARM,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);


AlarmPlugin::AlarmPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(AlarmPlugin)), MQTTComponent(ComponentTypeEnum_t::LIGHT), _nextAlarm(0)
{
    REGISTER_PLUGIN(this, "AlarmPlugin");
}

void AlarmPlugin::setup(SetupModeType mode)
{
    _debug_println();
    _installAlarms();
    addTimeUpdatedCallback(ntpCallback);
    // MQTT has lower priority, call register component later
    LoopFunctions::callOnce([this]() {
        MQTTClient::safeRegisterComponent(this);
    });
}

void AlarmPlugin::reconfigure(const String &source)
{
    _debug_println();
    if (String_equals(source, SPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
    else {
        _removeAlarms();
        _installAlarms();
    }
}

void AlarmPlugin::shutdown()
{
    MQTTClient::safeUnregisterComponent(this);
    _removeAlarms();
}

AlarmPlugin::MQTTAutoDiscoveryPtr AlarmPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, F("alarm"), format);
            discovery->addStateTopic(MQTTClient::formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(MQTTClient::formatTopic(FSPGM(_set)));
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            break;
    }
    discovery->finalize();
    return discovery;
}

void AlarmPlugin::onConnect(MQTTClient *client)
{
    client->subscribe(this, MQTTClient::formatTopic(FSPGM(_set)), MQTTClient::getDefaultQos());
}

void AlarmPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    if (_callback) {
        auto value = (bool)atoi(payload);
        if (value) {
            _callback(Alarm::AlarmModeType::BOTH, Alarm::DEFAULT_MAX_DURATION);
        }
        else {
            _callback(Alarm::AlarmModeType::BOTH, Alarm::STOP_ALARM);
        }
        client->publish(MQTTClient::formatTopic(FSPGM(_state)), MQTTClient::getDefaultQos(), true, String(value));
    }
}

static uint8_t form_get_alarm_num(const String &varName)
{
#if IOT_ALARM_PLUGIN_MAX_ALERTS <= 10
    return (varName.charAt(1) - '0');
#else
    return (uint8_t)atoi(varName.c_str() + 1);
#endif
}

static bool form_enabled_callback(bool value, FormField &field, bool store)
{
    if (store) {
        auto alarmNum = form_get_alarm_num(field.getName());
        // _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
        auto &cfg = AlarmPlugin::Alarm::getWriteableConfig();
        cfg.alarms[alarmNum].is_enabled = value;
    }
    return false;
}

static bool form_weekday_callback(uint8_t value, FormField &field, bool store)
{
    if (store) {
        auto alarmNum = form_get_alarm_num(field.getName());
        // _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
        auto &cfg = AlarmPlugin::Alarm::getWriteableConfig();
        cfg.alarms[alarmNum].time.week_day.week_days = value;
    }
    return false;
}

static bool form_duration_callback(uint16_t value, FormField &field, bool store)
{
    if (store) {
        auto alarmNum = form_get_alarm_num(field.getName());
        // _debug_printf_P(PSTR("name=%s alarm=%u store=%u value=%u\n"), field.getName().c_str(), alarmNum, store, value);
        auto &cfg = AlarmPlugin::Alarm::getWriteableConfig();
        cfg.alarms[alarmNum].max_duration = value;
    }
    return false;
}

void AlarmPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u alarm(s) set"), _alarms.size());

    if (_nextAlarm) {
        char buf[32];
        time_t _now = (time_t)_nextAlarm;
        strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), localtime(&_now));
        output.print(F(", next at "));
        output.print(buf);
    }
}

void AlarmPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("type=%u name=%s foprm=%p request=%p\n"), type, formName.c_str(), &form, request);
    if (type == FormCallbackType::SAVE) {
        auto &cfg = Alarm::getWriteableConfig();
        auto now = time(nullptr) + 30;
        Alarm::updateTimestamps(localtime(&now), cfg);
#if DEBUG_ALARM_FORM
        Alarm::dump(DEBUG_OUTPUT, cfg);
#endif
    }
    else if (isCreateFormCallbackType(type)) {
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

            // TODO check packed struct cannot be accessed as ref/by ptr
            form.add<uint8_t>(prefix + String('h'), &alarm.time.hour);
            form.add<uint8_t>(prefix + String('m'), &alarm.time.minute);
#if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
            form.add<uint8_t>(prefix + String('t'), &alarm.mode);
#else
            alarm.mode_type = Alarm::AlarmModeType::BOTH;
#endif
            form.add<uint16_t>(prefix + String('d'), alarm.max_duration, form_duration_callback);

            form.add<uint8_t>(prefix + String('w'), alarm.time.week_day.week_days, form_weekday_callback);
        }

        form.finalize();
    }
}

void AlarmPlugin::resetAlarm()
{
    MQTTClient::safePublish(MQTTClient::formatTopic(FSPGM(_state)), true, String(0));
}

void AlarmPlugin::setCallback(Callback callback)
{
    plugin._callback = callback;
}

void AlarmPlugin::ntpCallback(time_t now)
{
    plugin._ntpCallback(now);
}

void AlarmPlugin::timerCallback(EventScheduler::TimerPtr timer)
{
    plugin._timerCallback(timer);
}

void AlarmPlugin::_installAlarms(EventScheduler::TimerPtr timer)
{
    _debug_println();
    Alarm::TimeType delay = 300;
    Alarm::TimeType minAlarmTime = std::numeric_limits<Alarm::TimeType>::max();
    _nextAlarm = 0;

    if (!IS_TIME_VALID(time(nullptr))) {
        _debug_printf_P(PSTR("time not valid: %u\n"), (int)time(nullptr));
    }
    else {
        auto now = time(nullptr) + 30;
        const auto tm = *localtime(&now); // we need a copy in ase the debug code is active and calls localtime() again
        now -= 30; // remove safety margin
        auto cfg = Alarm::getConfig();
        for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
            auto &alarm = cfg.alarms[i];
            auto alarmTime = Alarm::getTime(&tm, alarm);

#if DEBUG_ALARM_FORM
            char buf[32] = { 0 };
            time_t _now = (time_t)alarmTime;
            if (_now) {
                strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), localtime(&_now));
            }
            _debug_printf_P(PSTR("alarm %u: enabled=%u alarm_time=%u%s time=%02u:%02u duration=%u ts=%u mode=%u weekdays=%s\n"),
                i, alarm.is_enabled, (int)alarmTime, buf, alarm.time.hour, alarm.time.minute, alarm.max_duration,
                (int)alarm.time.timestamp, alarm.mode, Alarm::getWeekDaysString(alarm.time.week_day.week_days).c_str()
            );
#endif

            if (alarmTime && alarmTime >= static_cast<Alarm::TimeType>(now)) {
                minAlarmTime = std::min(minAlarmTime, alarmTime);
                _alarms.emplace_back(alarmTime, alarm);
            }
        }

        if (!_alarms.empty()) {
            _nextAlarm = minAlarmTime;
            delay = minAlarmTime - now;
            if (delay > 360) {
                delay = 300;
            }
            else if (delay < 1) {
                delay = 1;
            }
        }
    }

    // run timer every 5min. in case time changed and we missed it
    // daylight savings time or any other issue with time changing without a callback
    _debug_printf_P(PSTR("timer delay=%u min_time=%d alarms=%u\n"), (int)delay, (int)minAlarmTime, _alarms.size());
    if (timer) {
        timer->rearm(delay * 1000UL); // rearm timer inside timer callback
    }
    else {
        _timer.add(delay * 1000UL, true, timerCallback);
    }
}

void AlarmPlugin::_removeAlarms()
{
    _debug_println();
    _nextAlarm = 0;
    _timer.remove();
    _alarms.clear();
}

void AlarmPlugin::_ntpCallback(time_t now)
{
    _debug_printf_P(PSTR("time=%u\n"), (int)now);
    if (IS_TIME_VALID(now)) {
        // reinstall alarms if time changed
        _removeAlarms();
        _installAlarms();
    }
}

void AlarmPlugin::_timerCallback(EventScheduler::TimerPtr timer)
{
    _debug_println();
    if (!_alarms.empty()) {
        bool store = false;
        bool triggered = false;
        auto now = time(nullptr) + 30;
        for(auto &alarm: _alarms) {
            if (alarm._time && static_cast<Alarm::TimeType>(now) >= alarm._time) {
                auto ts = alarm._alarm.time.timestamp;
                PrintString message = F("Alarm triggered");
                if (ts) {
                    message.print(F(", one time"));
                }
                if (alarm._alarm.mode_type == Alarm::AlarmModeType::SILENT) {
                    message.print(F(", silent"));
                }
                else if (alarm._alarm.mode_type == Alarm::AlarmModeType::BUZZER) {
                    message.print(F(", buzzer"));
                }
                Logger_notice(message);
                if (_callback) {
                    triggered = true;
                    _callback(alarm._alarm.mode_type, alarm._alarm.max_duration);
                }

                if (ts) { // remove one time alarms
                    auto &cfg = Alarm::getWriteableConfig();
                    for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
                        auto &alarm = cfg.alarms[i];
                        if (alarm.time.timestamp == ts) {
                            alarm.time.timestamp = 0;
                            alarm.is_enabled = false;
                            store = true;
                        }
                    }
                }
            }
        }
        if (store) {
            config.write();
        }
        _alarms.clear();
        if (triggered) {
            MQTTClient::safePublish(MQTTClient::formatTopic(FSPGM(_state)), true, String(1));
        }
    }

    _installAlarms(timer);
}
