/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

#ifndef IOT_SENSOR_AMBIENT_LIGHT_RENDER_TYPE
#define IOT_SENSOR_AMBIENT_LIGHT_RENDER_TYPE WebUINS::SensorRenderType::COLUMN
#endif

#ifndef IOT_SENSOR_AMBIENT_LIGHT_RENDER_HEIGHT
#define IOT_SENSOR_AMBIENT_LIGHT_RENDER_HEIGHT F("15rem")
#endif

#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2
// auto initialize illuminance sensor
#   ifndef IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2_BH1750FVI_I2C_ADDRESS
// #    define IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2_BH1750FVI_I2C_ADDRESS 0x23
#   endif
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

    // https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf

    static constexpr uint8_t BH1750FVI_ADDR_LOW =       0b00100011; // 0x23
    static constexpr uint8_t BH1750FVI_ADDR_HIGH =      0b01011100; // 0x5c

    static constexpr uint8_t BH1750FVI_POWER_DOWN =     0b00000000;
    static constexpr uint8_t BH1750FVI_POWER_UP =       0b00000001;
    static constexpr uint8_t BH1750FVI_RESET =          0b00000111;

    // Continuously
    static constexpr uint8_t BH1750FVI_MODE_HIGHRES =   0b00010000;
    static constexpr uint8_t BH1750FVI_MODE_HIGHRES2 =  0b00010001;
    static constexpr uint8_t BH1750FVI_MODE_LOWRES =    0b00010011;

    // Measurement Time Register
    static constexpr uint8_t BH1750FVI_MTREG_DEFAULT =  0b01000101; // 69

     static constexpr uint8_t BH1750FVI_MTREG_HIGH(uint8_t value) {
        return 0b01000000 | (value >> 5);
    }
    static constexpr uint8_t BH1750FVI_MTREG_LOW(uint8_t value) {
        return 0b01100000 | (value & 0b11111);
    }

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
            // use TinyPWM to read a photo resistor connected to one of the ADC pins
            // measurement time for the value (integration time) is 2 seconds
            TinyPWM(uint8_t _i2cAddress, bool _inverted = true, uint8_t _adcPin = 0x11) : level(NAN), lastUpdate(0), i2cAddress(_i2cAddress), inverted(_inverted), adcPin(_adcPin) {}
        };

        struct BH1750FVI {
            float illuminance;
            uint8_t i2cAddress;
            bool highRes;
            BH1750FVI() {}
            // highres mode displays lux (0.5lx, 1-65535) and is not suitable for auto brightness
            // the update interval is 1 second
            // the optical window can be changed to measure up to 100000lux with 16bit precision
            // low res mode is 4lx and updated every 125ms, the available range is 0-1023
            BH1750FVI(uint8_t _i2cAddress, bool _highRes) : illuminance(NAN), i2cAddress(_i2cAddress), highRes(_highRes) {}
        };

        struct ADC {
            bool inverted;
            ADC() {}
            // use the internal ADC to read a photo resistor or another resistive device
            // the value is an average over 24 measurements every 30 milliseconds
            ADC(bool _inverted) : inverted(_inverted) {}
        };

        SensorType type;
        union {
            TinyPWM tinyPWM;
            BH1750FVI bh1750FVI;
            ADC adc;
        };
        SensorConfig() {}
        SensorConfig(SensorType _type) : type(_type) {}
    };

public:
    Sensor_AmbientLight(const String &name, uint8_t id = 0);
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
    uint8_t getId() const;
    bool enabled() const;

private:
    void _timerCallback();
    String _getId();
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
    uint8_t _id;
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

inline uint8_t Sensor_AmbientLight::getId() const
{
    return _id;
}

inline bool Sensor_AmbientLight::enabled() const
{
    return _handler != nullptr && _timer != false;
}

#endif
