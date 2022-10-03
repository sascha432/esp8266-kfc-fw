/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <EventScheduler.h>
#include "Form.h"
#include "PluginComponent.h"
#include "plugins_menu.h"
#include "kfc_fw_config.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/ntp/ntp_plugin.h"

#if DEBUG_ALARM_FORM
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#ifndef DEBUG_ALARM_FORM
#   define DEBUG_ALARM_FORM 0
#endif

#if !defined(IOT_ALARM_PLUGIN_ENABLED) || !IOT_ALARM_PLUGIN_ENABLED
#   error requires IOT_ALARM_PLUGIN_ENABLED=1
#endif

#if !NTP_HAVE_CALLBACKS
#   error requires NTP_HAVE_CALLBACKS=1
#endif

#ifndef IOT_ALARM_PLUGIN_HAS_BUZZER
#   define IOT_ALARM_PLUGIN_HAS_BUZZER 0
#endif

#ifndef IOT_ALARM_BUZZER_PIN
#    define IOT_ALARM_BUZZER_PIN -1
#endif

#ifndef IOT_ALARM_PLUGIN_HAS_SILENT
#   define IOT_ALARM_PLUGIN_HAS_SILENT 0
#endif

class AsyncWebServerRequest;

using namespace KFCConfigurationClasses::Plugins::AlarmConfigNS;

class AlarmPlugin : public PluginComponent, public MQTTComponent {
// PluginComponent
public:
    AlarmPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request);
    virtual void getStatus(Print &output) override;

// MQTTComponent
public:
    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 1;
    }
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len);

public:
    static void resetAlarm();
    static void setCallback(CallbackType callback);
    static bool getAlarmState();
    static void ntpCallback(time_t now);
    static void timerCallback(Event::CallbackTimerPtr timer);

public:
    enum class AlarmType : uint8_t {
        SILENT,
        BUZZER,
        BOTH,
    };

    #if IOT_ALARM_PLUGIN_HAS_BUZZER
        void setBuzzer(bool enabled);
        void turnBuzzerOn();
        void turnBuzzerOff();

        Event::Timer _buzzerTimer;
    #else
        void setBuzzer(bool enabled) {}
        void turnBuzzerOn() {}
        void turnBuzzerOff() {}
    #endif

    static AlarmPlugin &getInstance();

private:
    void _installAlarms(Event::CallbackTimerPtr timer);
    void _removeAlarms();
    void _ntpCallback(time_t now);
    void _timerCallback(Event::CallbackTimerPtr timer);
    void _publishState();

    inline static String _formatTopic(const __FlashStringHelper *topic) {
        return MQTT::Client::formatTopic(String(FSPGM(alarm)), topic);
    }

    class ActiveAlarm {
    public:
        ActiveAlarm(TimeType time, const SingleAlarmType &alarm) : _time(time), _alarm(alarm) {}

        TimeType _time;
        SingleAlarmType _alarm;
    };

    using ActiveAlarmVector = std::vector<ActiveAlarm>;

    Event::Timer _timer;
    ActiveAlarmVector _alarms;
    CallbackType _callback;
    TimeType _nextAlarm;
    bool _alarmState;
    int32_t _color;
};


inline void AlarmPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _installAlarms(*_timer);
    #if NTP_HAVE_CALLBACKS
        addTimeUpdatedCallback(ntpCallback);
    #endif
    MQTT::Client::registerComponent(this);
    #if IOT_ALARM_PLUGIN_HAS_BUZZER
        digitalWrite(IOT_ALARM_BUZZER_PIN, LOW);
        pinMode(IOT_ALARM_BUZZER_PIN, OUTPUT);
    #endif
}

inline void AlarmPlugin::onConnect()
{
    subscribe(_formatTopic(FSPGM(_set)));
    subscribe(_formatTopic(F("/rgb/set")));
    _publishState();
}

inline void AlarmPlugin::reconfigure(const String &source)
{
    _removeAlarms();
    _installAlarms(*_timer);
}

inline void AlarmPlugin::shutdown()
{
    MQTT::Client::unregisterComponent(this);
    _removeAlarms();
}

inline void AlarmPlugin::resetAlarm()
{
    __LDBG_printf("state=%u", getInstance()._alarmState);
    getInstance()._alarmState = false;
    getInstance()._publishState();
}

inline void AlarmPlugin::setCallback(CallbackType callback)
{
    getInstance()._callback = callback;
}

inline bool AlarmPlugin::getAlarmState()
{
    return getInstance()._alarmState;
}

inline void AlarmPlugin::ntpCallback(time_t now)
{
    __LDBG_printf("time=%u", (int)now);
    getInstance()._ntpCallback(now);
}

inline void AlarmPlugin::timerCallback(Event::CallbackTimerPtr timer)
{
    getInstance()._timerCallback(timer);
}

#if DEBUG_ALARM_FORM
#include <debug_helper_disable.h>
#endif
