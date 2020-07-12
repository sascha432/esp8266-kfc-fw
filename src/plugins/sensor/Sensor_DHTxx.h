/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_DHTxx

#ifndef IOT_SENSOR_HAVE_DHTxx_PIN
#define IOT_SENSOR_HAVE_DHTxx_PIN               5
#endif

#if IOT_SENSOR_HAVE_DHTxx == 11
#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx                  "DHT11"
#endif
#elif IOT_SENSOR_HAVE_DHTxx == 12
#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx                  "DHT12"
#endif
#elif IOT_SENSOR_HAVE_DHTxx == 21
#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx                  "AM2301"
#endif
#elif IOT_SENSOR_HAVE_DHTxx == 22
#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx                  "DHT22"
#endif
#else
#error Invalid IOT_SENSOR_HAVE_DHTxx
#endif

#include <Arduino_compat.h>
#include <Wire.h>
// #include <DHT.h>
#include <SimpleDHT.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

class Sensor_DHTxx : public MQTTSensor {
public:
    typedef struct {
        float temperature;  // °C
        float humidity;     // %
    } SensorData_t;

    typedef std::function<void(SensorData_t &data)> CompensationCallback_t;

    Sensor_DHTxx(const String &name, uint8_t pin/*, uint8_t type*/);

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    inline void readSensor(SensorData_t &data) {
        _readSensor(data);
    }

private:
    String _getId(const __FlashStringHelper *type = nullptr);
    void _readSensor(SensorData_t &data);

    String _name;
    uint8_t _pin;
#if IOT_SENSOR_HAVE_DHTxx == 11
    SimpleDHT11 _dht;
#elif IOT_SENSOR_HAVE_DHTxx == 22
    SimpleDHT22 _dht;
#else
    // https://github.com/adafruit/DHT-sensor-library
    DHT _dht;
    uint8_t _type;
#endif

};

#endif
