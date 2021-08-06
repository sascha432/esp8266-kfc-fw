/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_INA219

#include <Arduino_compat.h>
#include <Adafruit_INA219.h>
#include <Wire.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

using Plugins = KFCConfigurationClasses::PluginsType;

#ifndef IOT_SENSOR_INA219_RENDER_TYPE
#   define IOT_SENSOR_INA219_RENDER_TYPE WebUINS::SensorRenderType::ROW
#endif

#ifndef IOT_SENSOR_INA219_RENDER_HEIGHT
// #    define IOT_SENSOR_INA219_RENDER_HEIGHT F("15rem")
#endif

#ifndef IOT_SENSOR_INA219_R_SHUNT
#    define IOT_SENSOR_INA219_R_SHUNT 0.016
#endif

#ifndef IOT_SENSOR_INA219_GAIN
// 80mV range, max. 5A @ 0.016R
#    define IOT_SENSOR_INA219_GAIN INA219_CONFIG_GAIN_2_80MV
#endif

#ifndef IOT_SENSOR_INA219_BUS_URANGE
// bus voltage 32 = max. 26V
#    define IOT_SENSOR_INA219_BUS_URANGE INA219_CONFIG_BVOLTAGERANGE_32V
#endif

#ifndef IOT_SENSOR_INA219_SHUNT_ADC_RES
// average 128x 12bit samples
#    define IOT_SENSOR_INA219_SHUNT_ADC_RES INA219_CONFIG_SADCRES_12BIT_128S_69MS
#endif

#ifndef IOT_SENSOR_INA219_READ_INTERVAL
// should close to the sample/averaging rate
#    define IOT_SENSOR_INA219_READ_INTERVAL 68
#endif

#pragma push_macro("_U")
#undef _U

class Sensor_INA219 : public MQTT::Sensor {
public:
    enum class SensorInputType : char {
        VOLTAGE =       'u',
        CURRENT =       'i',
        POWER =         'p',
        AVG_CURRENT =   'j',
        AVG_POWER =     'o',
        PEAK_CURRENT =  'm',
        PEAK_POWER =    'n',
    };

    using ConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::INA219ConfigType;
    using CurrentDisplayType = KFCConfigurationClasses::Plugins::SensorConfigNS::INA219CurrentDisplayType;
    using PowerDisplayType = KFCConfigurationClasses::Plugins::SensorConfigNS::INA219PowerDisplayType;

public:
    Sensor_INA219(const String &name, TwoWire &wire, uint8_t address = IOT_SENSOR_HAVE_INA219);
    virtual ~Sensor_INA219();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

    virtual bool hasForm() const;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    virtual void reconfigure(PGM_P source) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    Adafruit_INA219 &getSensor();

    // average values
    float getVoltage() const;
    float getCurrent() const;
    float getPower() const;
    float getPeakCurrent() const;
    float getPeakPower() const;

    void resetPeak();

private:
    class SensorData {
    public:
        SensorData(uint32_t period);

        float U() const;
        float I() const;
        float P() const;

        void add(float U, float I, uint32_t micros);
        void set(float U, float I, uint32_t micros);
        void clear();

    private:
        float _U;
        float _I;
        uint32_t _micros;
        uint32_t _period;
    };

    void _loop();
    String _getId(SensorInputType type) const;
    ConfigType _readConfig() const;
    const __FlashStringHelper *_getCurrentUnit() const;
    const __FlashStringHelper *_getPowerUnit() const;
    uint8_t _getCurrentPrecision() const;
    uint8_t _getPowerPrecision() const;
    float _convertCurrent(float current) const;
    float _convertPower(float power) const;

    String _name;
    uint8_t _address;
    ConfigType _config;

    uint32_t _updateTimer;
    uint32_t _holdPeakTimer;
    SensorData _data;
    SensorData _avgData;
    SensorData _mqttData;
    float _Ipeak;
    float _Ppeak;

    Adafruit_INA219 _ina219;
};

inline Sensor_INA219::SensorData::SensorData(uint32_t period) : _U(NAN), _I(NAN), _period(period)
{
}

float inline Sensor_INA219::SensorData::U() const
{
    return _U;
}

inline float Sensor_INA219::SensorData::I() const
{
    return _I;
}

inline float Sensor_INA219::SensorData::P() const
{
    return isnan(_U) || isnan(_I) ? NAN : (_U * _I);
}

inline void Sensor_INA219::SensorData::add(float U, float I, uint32_t micros)
{
    if (isnan(_U) || isnan(_I)) {
        _U = U;
        _I = I;
    } else {
        float multiplier = _period / (get_time_diff(_micros, micros) / 1000.0);
        float divider = multiplier + 1.0;
        _U = ((_U * multiplier) + U) / divider;
        _I = ((_I * multiplier) + I) / divider;
        // __DBG_printf("U=%f I=%f U=%f I=%f m=%f d=%f", U, I, _U, _I, multiplier, divider);
    }
    _micros = micros;
}

inline void Sensor_INA219::SensorData::set(float U, float I, uint32_t micros)
{
    _U = U;
    _I = I;
    _micros = micros;
}

inline void Sensor_INA219::SensorData::clear()
{
    _U = NAN;
    _I = NAN;
}

inline Adafruit_INA219 &Sensor_INA219::getSensor()
{
    return _ina219;
}

// average values
inline float Sensor_INA219::getVoltage() const
{
    return _data.U();
}

inline float Sensor_INA219::getCurrent() const
{
    return _data.I();
}

inline float Sensor_INA219::getPower() const
{
    return _data.P();
}

inline float Sensor_INA219::getPeakCurrent() const
{
    return _Ipeak;
}

inline float Sensor_INA219::getPeakPower() const
{
    return _Ppeak;
}

inline void Sensor_INA219::resetPeak()
{
    _Ipeak = NAN;
    _Ppeak = NAN;
    _holdPeakTimer = 0;
}

inline uint8_t Sensor_INA219::getAutoDiscoveryCount() const
{
    return 5;
}

inline bool Sensor_INA219::hasForm() const
{
    return true;
}

#pragma pop_macro("_U")

#endif
