/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_BME280

#include <Arduino_compat.h>
#include <Wire.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include <Adafruit_BME280.h>

#ifndef IOT_SENSOR_BME280_HAVE_COMPENSATION_CALLBACK
#define IOT_SENSOR_BME280_HAVE_COMPENSATION_CALLBACK 0
#endif

#ifndef IOT_SENSOR_BME280_RENDER_TYPE
#define IOT_SENSOR_BME280_RENDER_TYPE WebUINS::SensorRenderType::ROW
#endif

#ifndef IOT_SENSOR_BME280_RENDER_HEIGHT
// #define IOT_SENSOR_BME280_RENDER_HEIGHT F("15rem")
#endif

class Sensor_CCS811;

// using namespace KFCConfigurationClasses::Plugins;

class Sensor_BME280 : public MQTT::Sensor {
public:
    struct SensorDataType {
        float temperature;  // °C
        float humidity;     // %
        float pressure;     // hPa

        SensorDataType() {}
        SensorDataType(float _temperature, float _humidity, float _pressure) : temperature(_temperature), humidity(_humidity), pressure(_pressure) {}
    };

    using Plugins = KFCConfigurationClasses::PluginsType;
    using CompensationCallback = std::function<void(SensorDataType &data)>;
    using SensorConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::BME280SensorType;

public:
    Sensor_BME280(const String &name, TwoWire &wire, uint8_t address = 0x76);
    virtual ~Sensor_BME280();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual bool hasForm() const;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    virtual void reconfigure(PGM_P source) override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    SensorDataType readSensor();

#if IOT_SENSOR_BME280_HAVE_COMPENSATION_CALLBACK
    // temperature or offset to compensate temperature and humidity readings
    void setCompensationCallback(CompensationCallback callback);
#endif

private:
    friend Sensor_CCS811;

    String _getId(const __FlashStringHelper *type = nullptr);
    SensorDataType _readSensor();
    void _readConfig();

    String _name;
    uint8_t _address;
    Adafruit_BME280 _bme280;
    CompensationCallback _callback;
    SensorConfigType _cfg;
};

inline uint8_t Sensor_BME280::getAutoDiscoveryCount() const
{
    return 3;
}

inline Sensor_BME280::SensorDataType Sensor_BME280::readSensor()
{
    return _readSensor();
}

#if IOT_SENSOR_BME280_HAVE_COMPENSATION_CALLBACK

inline void Sensor_BME280::setCompensationCallback(CompensationCallback callback)
{
    _callback = callback;
}

#endif

inline bool Sensor_BME280::hasForm() const
{
    return true;
}

inline void Sensor_BME280::reconfigure(PGM_P source)
{
    _readConfig();
}

inline void Sensor_BME280::_readConfig()
{
    _cfg = Plugins::Sensor::getConfig().bme280;
}

inline String Sensor_BME280::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("bme280_0x%02x"), _address);
    if (type) {
        id.print('_');
        id.print(type);
    }
    return id;
}

#endif
