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

class Sensor_CCS811;

class Sensor_BME280 : public MQTT::Sensor {
public:
    typedef struct {
        float temperature;  // °C
        float humidity;     // %
        float pressure;     // hPa
    } SensorData_t;

    typedef std::function<void(SensorData_t &data)> CompensationCallback_t;

    Sensor_BME280(const String &name, TwoWire &wire, uint8_t address = 0x76);
    virtual ~Sensor_BME280();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    inline void readSensor(SensorData_t &data) {
        _readSensor(data);
    }

    // temperature or offset to compensate temperature and humidity readings
    void setCompensationCallback(CompensationCallback_t callback) {
        _callback = callback;
    }

private:
    friend Sensor_CCS811;

    String _getId(const __FlashStringHelper *type = nullptr);
    void _readSensor(SensorData_t &data);

    String _name;
    uint8_t _address;
    Adafruit_BME280 _bme280;
    CompensationCallback_t _callback;
};

#endif
