/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <EventScheduler.h>
#include "Form.h"
#include "PluginComponent.h"
#include "plugins_menu.h"
#include "kfc_fw_config_classes.h"

#ifndef DEBUG_ALARM_FORM
#define DEBUG_ALARM_FORM                                1
#endif

#if !NTP_HAVE_CALLBACKS
#error requires NTP_HAVE_CALLBACKS=1
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
    using Alarm = KFCConfigurationClasses::Plugins::Alarm;
    using Callback = std::function<void(Alarm::AlarmModeType mode, uint16_t maxDuration)>;

    AlarmPlugin();

    virtual PGM_P getName() const {
        return PSTR("alarm");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Alarm");
    }
    virtual PriorityType getSetupPriority() const override {
        return PriorityType::ALARM;
    }

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual void configurationSaved(Form *form) override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

public:
    static void setCallback(Callback callback);
    static void ntpCallback(time_t now);
    static void timerCallback(EventScheduler::TimerPtr timer);

public:
    enum class AlarmType : uint8_t {
        SILENT,
        BUZZER,
        BOTH,
    };

private:
    void _installAlarms(EventScheduler::TimerPtr = nullptr);
    void _removeAlarms();
    void _ntpCallback(time_t now);
    void _timerCallback(EventScheduler::TimerPtr timer);

    class ActiveAlarm {
    public:
        ActiveAlarm(Alarm::TimeType time, const Alarm::SingleAlarm_t &alarm) : _time(time), _alarm(alarm) {}

        Alarm::TimeType _time;
        Alarm::SingleAlarm_t _alarm;
    };

    using ActiveAlarmVector = std::vector<ActiveAlarm>;

    EventScheduler::Timer _timer;
    ActiveAlarmVector _alarms;
    Callback _callback;
    Alarm::TimeType _nextAlarm;
};
