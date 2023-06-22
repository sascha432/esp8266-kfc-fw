/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_MOTION_SENSOR

#include "MQTTSensor.h"
#include "WebUIComponent.h"
#include "plugins.h"
#include <Arduino_compat.h>

#ifndef IOT_SENSOR_MOTION_RENDER_TYPE
#    define IOT_SENSOR_MOTION_RENDER_TYPE WebUINS::SensorRenderType::COLUMN
#endif

#ifndef IOT_SENSOR_MOTION_RENDER_HEIGHT
#    define IOT_SENSOR_MOTION_RENDER_HEIGHT F("15rem")
#endif

#ifndef IOT_SENSOR_MOTION_POLL_INTERVAL
#    define IOT_SENSOR_MOTION_POLL_INTERVAL Event::seconds(1)
#endif

class Sensor_Motion;

class MotionSensorHandler {
public:
    // motion = true: gets called once motion is detected
    // motion = false: gets called once motion has not been detected for the configured timeout
    virtual void eventMotionDetected(bool motion) {}

#if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
    MotionSensorHandler(bool autoOff = false) : _autoOff(autoOff) {}

    // state = true: turn device off, the auto off timeout without motion has been reached
    // state = false: turn device on, motion was detected
    // return value true will result in logging the event
    virtual bool eventMotionAutoOff(bool state) = 0;

    // set state for auto off
    void _setMotionAutoOff(bool autoOff) {
        _autoOff = autoOff;
    }

    bool _getMotionAutoOff() const {
        return _autoOff;
    }

private:
    friend Sensor_Motion;

    Event::Timer _autoOffTimeout;
    bool _autoOff;
#endif
};

class Sensor_Motion : public MQTT::Sensor {
public:
    using Plugins = KFCConfigurationClasses::PluginsType;
    using ConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::MotionSensorConfigType;

    static constexpr auto kPollPinInterval = IOT_SENSOR_MOTION_POLL_INTERVAL;

public:
    Sensor_Motion(const String &name);
    virtual ~Sensor_Motion();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

    void begin(MotionSensorHandler *handler, uint8_t pin, bool pinInverted = false);
    void end();

    bool getMotionState() const;

private:
    void _reset(bool shutdown = false);
    void _timerCallback();
    const __FlashStringHelper *_getId();
    String _getTopic();
    const __FlashStringHelper *_getStateStr(uint8_t state) const;
    void _publishWebUI();

private:
    String _name;
    MotionSensorHandler *_handler;
    Event::Timer _timer;
    Event::Timer _sensorTimeout;
    ConfigType _config;
    bool _motionState;
    uint8_t _pin;
    bool _pinInverted;
};

inline bool Sensor_Motion::getMotionState() const
{
    return _motionState;
}

inline const __FlashStringHelper *Sensor_Motion::_getId()
{
    return F("motion");
}

inline String Sensor_Motion::_getTopic()
{
    return MQTT::Client::formatTopic(_getId());
}

inline const __FlashStringHelper *Sensor_Motion::_getStateStr(uint8_t state) const
{
    switch(state) {
        case 1:
            return F("ON");
        case 0:
            return F("OFF");
        default:
            break;
    }
    return F("DISABLED");
}

#endif
