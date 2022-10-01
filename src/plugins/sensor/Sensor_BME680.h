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
#if HAVE_ADAFRUIT_BME680_LIB
#    include <Adafruit_BME680.h>
#endif
#if HAVE_BOSCHSENSORTEC_LIB
#    include "bsec.h"
#endif

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
        float gas;       // VOC in ppm
        uint32_t lastSuccess;

        SensorDataType(float _temperature = NAN, float _humidity = NAN, float _pressure = NAN, float _gas = NAN) :
            temperature(_temperature),
            humidity(_humidity),
            pressure(_pressure),
            gas(_gas),
            lastSuccess(0)
        {}

        void setLastSuccess() {
            lastSuccess = millis();
        }

        uint32_t getTimeSinceLastSuccess() const {
            return millis() - lastSuccess;
        }
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
    SensorDataType &_readSensor();

    void _readConfig();

    String _name;
    uint8_t _address;
    #if HAVE_ADAFRUIT_BME680_LIB
        Adafruit_BME680 _bme680;
    #endif
    #if HAVE_BOSCHSENSORTEC_LIB
        Bsec iaqSensor;
    #endif
    SensorConfigType _cfg;
    SensorDataType _sensor;
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

#if HAVE_ADAFRUIT_BME680_LIB

inline void Sensor_BME680::setup()
{
    _readConfig();
    if (!_bme680.begin(_address)) {
        return;
    }
    _bme680.setTemperatureOversampling(BME680_OS_8X);
    _bme680.setHumidityOversampling(BME680_OS_2X);
    _bme680.setPressureOversampling(BME680_OS_4X);
    _bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
    _bme680.setGasHeater(320, 150); // 320*C for 150 ms
}

#endif

#if HAVE_BOSCHSENSORTEC_LIB


inline void Sensor_BME680::setup()
{
    _readConfig();
    iaqSensor.begin(_address, config.initTwoWire());

    bsec_virtual_sensor_t sensorList[10] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    iaqSensor.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);

    _readSensor();


#endif
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
