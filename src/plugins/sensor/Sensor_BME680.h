/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_BME680

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include <Adafruit_BME680.h>

#ifndef IOT_SENSOR_BME680_RENDER_TYPE
#   define IOT_SENSOR_BME680_RENDER_TYPE WebUINS::SensorRenderType::ROW
#endif

#ifndef IOT_SENSOR_BME680_RENDER_HEIGHT
// #    define IOT_SENSOR_BME680_RENDER_HEIGHT F("15rem")
#endif

class Sensor_CCS811;

class Sensor_BME680 : public MQTT::Sensor {
public:
    struct SensorDataType {
        float temperature;  // °C
        float humidity;     // %
        float pressure;     // hPa
        uint32_t gas;       // VOC in ppm

        SensorDataType(float _temperature = NAN, float _humidity = NAN, float _pressure = NAN, uint32_t _gas = 0) :
            temperature(_temperature),
            humidity(_humidity),
            pressure(_pressure),
            gas(_gas)
        {}
    };

    using Plugins = KFCConfigurationClasses::PluginsType;
    using SensorConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::BME680SensorType;

public:
    Sensor_BME680(const String &name, uint8_t address = 0x77, TwoWire &wire = Wire);
    virtual ~Sensor_BME680();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual bool hasForm() const;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    virtual void setup();
    virtual void reconfigure(PGM_P source) override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    SensorDataType readSensor();

private:
    friend Sensor_CCS811;

    String _getId(const __FlashStringHelper *type = nullptr);
    SensorDataType _readSensor();

    void _readConfig();

    String _name;
    uint8_t _address;
    Adafruit_BME680 _bme680;
    SensorConfigType _cfg;
};

inline uint8_t Sensor_BME680::getAutoDiscoveryCount() const
{
    return 4;
}

inline Sensor_BME680::SensorDataType Sensor_BME680::readSensor()
{
    return _readSensor();
}

inline bool Sensor_BME680::hasForm() const
{
    return true;
}

inline void Sensor_BME680::setup()
{
    _readConfig();
    _bme680.begin(_address);
}

inline void Sensor_BME680::reconfigure(PGM_P source)
{
    _readConfig();
}

inline void Sensor_BME680::_readConfig()
{
    _cfg = Plugins::Sensor::getConfig().bme680;
}

inline String Sensor_BME680::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("bme680_0x%02x"), _address);
    if (type) {
        id.print('_');
        id.print(type);
    }
    return id;
}

#endif
