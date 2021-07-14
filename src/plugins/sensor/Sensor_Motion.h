/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_MOTION_SENSOR

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

using KFCConfigurationClasses::Plugins;

class Sensor_Motion;

class MotionSensorHandler {
public:
    // motion = true: gets called once motion is detected
    // motion = false: gets called once motion has not been detected for the configured timeout
    virtual void eventMotionDetected(bool motion) {}

#if IOT_SENSOR_HAVE_MOTION_AUTO_OFF
    MotionSensorHandler(bool autoOff = true) : _autoOff(autoOff) {}

    // off = true: gets called when motion has not been detected for the configured timeout
    // off = false: gets called when motion has not been after auto off was called
    // return value true will result in logging the event
    virtual bool eventMotionAutoOff(bool off) = 0;

    // enable/disable auto off events
    void setMotionAutoOff(bool autoOff) {
        _autoOff = autoOff;
    }

private:
    friend Sensor_Motion;

    bool _autoOff;
#endif
};

class Sensor_Motion : public MQTT::Sensor {
public:
    using ConfigType = Plugins::SensorConfig::MotionSensorConfig_t;

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

private:
    void _timerCallback();
    const __FlashStringHelper *_getId();
    String _getTopic();
    const __FlashStringHelper *_getStateStr(uint8_t state) const;

private:
    String _name;
    MotionSensorHandler *_handler;
    Event::Timer _timer;
    ConfigType _config;
    uint32_t _motionLastUpdate;
    uint8_t _motionState;
    uint8_t _pin;
    bool _pinInverted;
};


inline const __FlashStringHelper *Sensor_Motion::_getId()
{
    return F("motion");
}

inline String Sensor_Motion::_getTopic()
{
    return MQTTClient::formatTopic(_getId());
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
