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
