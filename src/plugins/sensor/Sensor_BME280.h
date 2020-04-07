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

class Sensor_BME280 : public MQTTSensor {
public:
    typedef struct {
        float temperature;  // °C
        float humidity;     // %
        float pressure;     // hPa
    } SensorData_t;

    typedef std::function<void(SensorData_t &data)> CompensationCallback_t;

    Sensor_BME280(const String &name, TwoWire &wire, uint8_t address = 0x76);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;

    inline SensorData_t readSensor() {
        return _readSensor();
    }

    // temperature or offset to compensate temperature and humidity readings
    void setCompensationCallback(CompensationCallback_t callback) {
        _callback = callback;
    }

private:
    friend Sensor_CCS811;

    String _getId(const __FlashStringHelper *type = nullptr);
    SensorData_t _readSensor();

    String _name;
    uint8_t _address;
    Adafruit_BME280 _bme280;
    CompensationCallback_t _callback;
};

#endif
