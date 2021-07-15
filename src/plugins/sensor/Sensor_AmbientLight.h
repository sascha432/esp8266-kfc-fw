/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

// support for TinyPWM I2C ADC
#ifndef IOT_SENSOR_AMBIENT_HAVE_TINYPWM
#define IOT_SENSOR_AMBIENT_HAVE_TINYPWM 0
#endif

#ifndef IOT_SENSOR_AMBIENT_TINYPWM_I2C_ADDRESS
#define IOT_SENSOR_AMBIENT_TINYPWM_I2C_ADDRESS TINYPWM_I2C_ADDRESS
#endif

#ifndef IOT_SENSOR_AMBIENT_TINYPWM_ADC_PIN
#define IOT_SENSOR_AMBIENT_TINYPWM_ADC_PIN 0x11
#endif

// support for ADC
#ifndef IOT_SENSOR_AMBIENT_HAVE_ADC
#define IOT_SENSOR_AMBIENT_HAVE_ADC 1
#endif

using KFCConfigurationClasses::Plugins;

class Sensor_AmbientLight;

class AmbientLightSensorHandler {
public:

    AmbientLightSensorHandler() :
        _autobrightness(true),
        _autoBrightnessValue(1.0f),
        _ambientLightSensor(nullptr)
    {
    }

    void setAutobrightness(bool autobrightness) {
        _autobrightness = autobrightness;
        if (!autobrightness) {
            _autoBrightnessValue = 1.0;
        }
    }

    bool isAutobrightnessEnabled() const {
        return _autobrightness;
    }

    float getAutoBrightness() const {
        return _autoBrightnessValue;
    }

private:
    friend Sensor_AmbientLight;

    bool _autobrightness;
    float _autoBrightnessValue;
    Sensor_AmbientLight *_ambientLightSensor;
};

class Sensor_AmbientLight : public MQTT::Sensor {
public:
    using ConfigType = Plugins::SensorConfig::AmbientLightSensorConfig_t;

    static constexpr int16_t kAutoBrightnessOff = -1;

    static constexpr uint8_t BH1750FVI_ADDR_LOW = 0x23;
    static constexpr uint8_t BH1750FVI_ADDR_HIGH = 0x5c;

    static constexpr uint8_t BH1750FVI_POWER_DOWN = 0x00;
    static constexpr uint8_t BH1750FVI_POWER_UP = 0x01;
    static constexpr uint8_t BH1750FVI_RESET = 0x07;

    static constexpr uint8_t BH1750FVI_MODE_HIGHRES = 0x10;
    static constexpr uint8_t BH1750FVI_MODE_HIGHRES2 = 0x11;
    static constexpr uint8_t BH1750FVI_MODE_LOWRES = 0x13;

    enum class SensorType {
        NONE = 0,
        INTERNAL_ADC,
        TINYPWM,
        BH1750FVI,
    };

    struct SensorConfig {

        struct TinyPWM {
            float level;
            uint32_t lastUpdate;
            uint8_t i2cAddress;
            bool inverted;
            uint8_t adcPin;
            TinyPWM() {}
            TinyPWM(uint8_t _i2cAddress, bool _inverted = true, uint8_t _adcPin = 0x11) : level(NAN), lastUpdate(0), i2cAddress(_i2cAddress), inverted(_inverted), adcPin(_adcPin) {}
        };

        struct BH1750FVI {
            uint8_t i2cAddress;
            BH1750FVI() {}
            BH1750FVI(uint8_t _i2cAddress) : i2cAddress(_i2cAddress) {}
        };

        SensorType type;
        union {
            TinyPWM tinyPWM;
            BH1750FVI bh1750FVI;
        };
        SensorConfig() {}
        SensorConfig(SensorType _type) : type(_type) {}
    };

public:
    Sensor_AmbientLight(const String &name);
    virtual ~Sensor_AmbientLight();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

    virtual bool hasForm() const;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

    void begin(AmbientLightSensorHandler *handler, const SensorConfig &sensor);
    void end();

    int32_t getValue() const;
    float getAutoBrightness() const;

private:
    void _timerCallback(uint32_t interval);
    const __FlashStringHelper *_getId();
    String _getTopic();
    String _getLightSensorWebUIValue();
    void _updateLightSensorWebUI();

    int32_t _readTinyPwmADC();
    int32_t _readBH1750FVI();

private:
    String _name;
    AmbientLightSensorHandler *_handler;
    Event::Timer _timer;
    SensorConfig _sensor;
    ConfigType _config;
    int32_t _value;
};

inline uint8_t Sensor_AmbientLight::getAutoDiscoveryCount() const
{
    return 1;
}

inline bool Sensor_AmbientLight::hasForm() const
{
    return true;
}

inline int32_t Sensor_AmbientLight::getValue() const
{
    return _value;
}

inline float Sensor_AmbientLight::getAutoBrightness() const
{
    return _handler ? _handler->_autoBrightnessValue * 100.0f : 1;
}

#endif
