/**
 * Author: sascha_lammers@gmx.de
 */


#include <PrintHtmlEntitiesString.h>
#include <ESPAsyncWebServer.h>
#include <EventScheduler.h>
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


AlarmPlugin::AlarmPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(AlarmPlugin)), MQTTComponent(ComponentTypeEnum_t::SWITCH), _nextAlarm(0), _alarmState(false)
{
    REGISTER_PLUGIN(this, "AlarmPlugin");
}

void AlarmPlugin::setup(SetupModeType mode)
{
    _debug_println();
    _installAlarms(*_timer);
    addTimeUpdatedCallback(ntpCallback);
    dependsOn(FSPGM(mqtt), [this](const PluginComponent *plugin) {
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
        _installAlarms(*_timer);
    }
}

void AlarmPlugin::shutdown()
{
   _debug_println();
     MQTTClient::safeUnregisterComponent(this);
    _removeAlarms();
}

AlarmPlugin::MQTTAutoDiscoveryPtr AlarmPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, FSPGM(alarm), format);
            discovery->addStateTopic(_formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(_formatTopic(FSPGM(_set)));
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            break;
    }
    discovery->finalize();
    return discovery;
}

void AlarmPlugin::onConnect(MQTTClient *client)
{
    client->subscribe(this, _formatTopic(FSPGM(_set)));
    _publishState();
}

void AlarmPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    __LDBG_printf("client=%p topic=%s payload=%s alarm_state=%u callback=%u", client, topic, payload, _alarmState, (bool)_callback);

    if (_callback) {
        _alarmState = (bool)atoi(payload);
        if (_alarmState) {
            _callback(Alarm::AlarmModeType::BOTH, Alarm::DEFAULT_MAX_DURATION);
        }
        else {
            _callback(Alarm::AlarmModeType::BOTH, Alarm::STOP_ALARM);
        }
        _publishState();
    }
}

void AlarmPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u alarm(s) set"), _alarms.size());

    if (_nextAlarm) {
        output.print(F(HTML_S(br) "Next at "));
        static_cast<PrintString &>(output).strftime(FSPGM(strftime_date_time_zone), _nextAlarm);
    }
    if (_alarmState) {
        output.print(F(HTML_S(br) "Alarm active"));
    }
}

#define FORM_CREATE_CALLBACK(name, field_name, type) \
    static bool form_##name##_callback(type value, FormUI::Field::Base &field, bool store) { \
        if (store) { \
            AlarmPlugin::Alarm::getWriteableConfig().alarms[atoi(field.getName().c_str() + 1)].field_name = value; \
        } \
        return false; \
    }

FORM_CREATE_CALLBACK(hour, time.hour, uint8_t);
FORM_CREATE_CALLBACK(minute, time.minute, uint8_t);
FORM_CREATE_CALLBACK(duration, max_duration, uint16_t);
FORM_CREATE_CALLBACK(weekday, time.week_day.week_days, uint8_t);
FORM_CREATE_CALLBACK(enabled, is_enabled, bool);
#if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
FORM_CREATE_CALLBACK(mode, mode, uint8_t);
#endif

void AlarmPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __LDBG_printf("type=%u name=%s form=%p request=%p", type, formName.c_str(), &form, request);
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

            form.add<bool>(prefix + 'e', alarm.is_enabled, form_enabled_callback);

            form.add<uint8_t>(prefix + 'h', alarm.time.hour, form_hour_callback);
            form.add<uint8_t>(prefix + 'm', alarm.time.minute, form_minute_callback);
#if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
            form.add<uint8_t>(prefix + String('t'), alarm.mode, form_mode_callback);
#else
            alarm.mode = Alarm::AlarmConfig::SingleAlarm_t::cast_int_mode(Alarm::AlarmModeType::BOTH);
#endif
            form.add<uint16_t>(prefix + 'd', alarm.max_duration, form_duration_callback);

            form.add<uint8_t>(prefix + 'w', alarm.time.week_day.week_days, form_weekday_callback);
        }

        form.finalize();
    }
}

void AlarmPlugin::resetAlarm()
{
    __LDBG_printf("state=%u", plugin._alarmState);
    plugin._alarmState = false;
    plugin._publishState();
}

void AlarmPlugin::setCallback(Callback callback)
{
    plugin._callback = callback;
}

bool AlarmPlugin::getAlarmState()
{
    return plugin._alarmState;
}

void AlarmPlugin::ntpCallback(time_t now)
{
    __LDBG_printf("time=%u", (int)now);
    plugin._ntpCallback(now);
}

void AlarmPlugin::timerCallback(Event::CallbackTimerPtr timer)
{
    plugin._timerCallback(timer);
}

void AlarmPlugin::_installAlarms(Event::CallbackTimerPtr timer)
{
    _debug_println();
    Alarm::TimeType delay = 300;
    Alarm::TimeType minAlarmTime = std::numeric_limits<Alarm::TimeType>::max();
    _nextAlarm = 0;

    if (!IS_TIME_VALID(time(nullptr))) {
        __LDBG_printf("time not valid: %u", (int)time(nullptr));
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
            __LDBG_printf("alarm %u: enabled=%u alarm_time=%u %s time=%02u:%02u duration=%u ts=%u mode=%u weekdays=%s",
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
    __LDBG_printf("timer delay=%u timer=%s min_time=%d alarms=%u", (int)delay, timer ? PSTR("rearm") : PSTR("add"), (int)minAlarmTime, _alarms.size());

    // Timer.add() is rearming if the timer exists
    _Timer(_timer).add(Event::seconds(delay), true, timerCallback);

    // if (timer) {
    //     timer->rearm(Event::seconds(delay));
    // }
    // else {
    //     _Timer(_timer).add(Event::seconds(delay), true, timerCallback);
    // }
}

void AlarmPlugin::_removeAlarms()
{
    _debug_println();
    _nextAlarm = 0;
    if (_timer) {
        _timer->disarm();
    }
    _alarms.clear();
}

void AlarmPlugin::_ntpCallback(time_t now)
{
    __LDBG_printf("time=%u", (int)now);
    if (IS_TIME_VALID(now)) {
        // reinstall alarms if time changed
        _removeAlarms();
        _installAlarms(*_timer);
    }
}

void AlarmPlugin::_timerCallback(Event::CallbackTimerPtr timer)
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
                if (Alarm::AlarmConfig::SingleAlarm_t::cast_enum_mode(alarm._alarm.mode) == Alarm::AlarmModeType::SILENT) {
                    message.print(F(", silent"));
                }
                else if (Alarm::AlarmConfig::SingleAlarm_t::cast_enum_mode(alarm._alarm.mode) == Alarm::AlarmModeType::BUZZER) {
                    message.print(F(", buzzer"));
                }
                Logger_notice(message);
                if (_callback) {
                    triggered = true;
                    _callback(Alarm::AlarmConfig::SingleAlarm_t::cast_enum_mode(alarm._alarm.mode), alarm._alarm.max_duration);
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
            _alarmState = true;
            _publishState();
        }
    }

    _installAlarms(timer);
}

void AlarmPlugin::_publishState()
{
    __LDBG_printf("publish state=%s", String((int)_alarmState).c_str());
     MQTTClient::safePublish(_formatTopic(FSPGM(_state)), true, String(_alarmState));
}

String AlarmPlugin::_formatTopic(const __FlashStringHelper *topic)
{
    return MQTTClient::formatTopic(FSPGM(alarm), topic);
}
