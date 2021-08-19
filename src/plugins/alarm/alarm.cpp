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
#include "Utility/ProgMemHelper.h"

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
    "",                 // reconfigure_dependencies
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


AlarmPlugin::AlarmPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(AlarmPlugin)),
    MQTTComponent(MQTT::ComponentType::LIGHT),
    _nextAlarm(0),
    _alarmState(false),
    _color(-1)
{
    REGISTER_PLUGIN(this, "AlarmPlugin");
}

MQTT::AutoDiscovery::EntityPtr AlarmPlugin::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    switch(num) {
        case 0:
            discovery->create(this, FSPGM(alarm), format);
            discovery->addStateTopic(_formatTopic(FSPGM(_state)));
            discovery->addCommandTopic(_formatTopic(FSPGM(_set)));
            discovery->addRGBStateTopic(_formatTopic(F("/rgb/state")));
            discovery->addRGBCommandTopic(_formatTopic(F("/rgb/set")));
            discovery->addName(F("Alarm"));
            break;
    }
    return discovery;
}

void AlarmPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("client=%p topic=%s payload=%s alarm_state=%u callback=%u", client, topic, payload, _alarmState, (bool)_callback);

    if (strcmp_P(topic, PSTR("/rgb/set")) == 0) {

        __DBG_printf("alarm color %s", payload);

    } else {

        if (_callback) {
            _alarmState = MQTT::Client::toBool(payload);
            if (_alarmState) {
                _callback(ModeType::BOTH, Alarm::DEFAULT_MAX_DURATION);
            }
            else {
                _callback(ModeType::BOTH, Alarm::STOP_ALARM);
            }
            _publishState();
        }
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

        auto &cfg = Alarm::getWriteableConfig();

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, 10, ae, ah, am, md, wd
            #if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
                , mt
            #endif
        );

        for(uint8_t i = 0; i < Alarm::MAX_ALARMS; i++) {
            //String prefix = 'a' + String(i);
            auto &alarm = cfg.alarms[i];

            form.addObjectGetterSetter(F_VAR(ae, i), alarm, alarm.get_bits_is_enabled, alarm.set_bits_is_enabled);
            form.addObjectGetterSetter(F_VAR(ah, i), alarm.time, alarm.time.get_bits_hour, alarm.time.set_bits_hour);
            form.addObjectGetterSetter(F_VAR(am, i), alarm.time, alarm.time.get_bits_minute, alarm.time.set_bits_minute);
            #if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
                form.addObjectGetterSetter(F_VAR(mt, i), alarm.mode, alarm.get_int_mode, alarm.set_int_mode);
            #else
                alarm.mode = SingleAlarmType::cast_int_mode(ModeType::BOTH);
            #endif

            form.addObjectGetterSetter(F_VAR(md, i), alarm, alarm.get_bits_max_duration, alarm.set_bits_max_duration);
            form.addObjectGetterSetter(F_VAR(wd, i), alarm.time, alarm.time.get_bits_weekdays, alarm.time.set_bits_weekdays);

/*
            --form.add<bool>(prefix + 'e', alarm.is_enabled, form_enabled_callback);
            --form.add<uint8_t>(prefix + 'h', alarm.time.hour, form_hour_callback);
            --form.add<uint8_t>(prefix + 'm', alarm.time.minute, form_minute_callback);
#if IOT_ALARM_PLUGIN_HAS_BUZZER && IOT_ALARM_PLUGIN_HAS_SILENT
            form.add<uint8_t>(prefix + String('t'), alarm.mode, form_mode_callback);
#else
            alarm.mode = Alarm::AlarmConfig::SingleAlarmType::cast_int_mode(Alarm::ModeType::BOTH);
#endif
            --form.add<uint16_t>(prefix + 'd', alarm.max_duration, form_duration_callback);

            --form.add<uint8_t>(prefix + 'w', alarm.time.week_day.week_days, form_weekday_callback);
*/
        }

        form.finalize();
    }
}

void AlarmPlugin::_installAlarms(Event::CallbackTimerPtr timer)
{
    _debug_println();
    TimeType delay = 300;
    TimeType minAlarmTime = std::numeric_limits<TimeType>::max();
    _nextAlarm = 0;

    if (!isTimeValid()) {
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

            if (alarmTime && alarmTime >= static_cast<TimeType>(now)) {
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
    if (isTimeValid(now)) {
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
            if (alarm._time && static_cast<TimeType>(now) >= alarm._time) {
                auto ts = alarm._alarm.time.timestamp;
                PrintString message = F("Alarm triggered");
                if (ts) {
                    message.print(F(", one time"));
                }
                if (SingleAlarmType::cast_enum_mode(alarm._alarm.mode) == ModeType::SILENT) {
                    message.print(F(", silent"));
                }
                else if (SingleAlarmType::cast_enum_mode(alarm._alarm.mode) == ModeType::BUZZER) {
                    message.print(F(", buzzer"));
                }
                Logger_notice(message);
                if (_callback) {
                    triggered = true;
                    _callback(SingleAlarmType::cast_enum_mode(alarm._alarm.mode), alarm._alarm.max_duration);
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
    if (isConnected()) {
        publish(_formatTopic(FSPGM(_state)), true, MQTT::Client::toBoolOnOff(_alarmState));
        publish(_formatTopic(F("/rgb/state")), true, PrintString(F("%06x"), _color));
    }
}

AlarmPlugin &AlarmPlugin::getInstance()
 {
     return plugin;
 }
