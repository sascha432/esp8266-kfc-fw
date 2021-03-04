/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_CCS811

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include <Adafruit_CCS811.h>

class Sensor_CCS811 : public MQTT::Sensor {
public:
    typedef struct {
        bool available;
        uint16_t eCO2;      // ppm
        uint16_t TVOC;      // ppb
    } SensorData_t;

    const uint8_t DEFAULT_UPDATE_RATE = MQTT::Sensor::DEFAULT_UPDATE_RATE;

    Sensor_CCS811(const String &name, uint8_t address = CCS811_ADDRESS);
    virtual ~Sensor_CCS811();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;

private:
    String _getId(const __FlashStringHelper *type = nullptr);
    SensorData_t &_readSensor();

    String _name;
    SensorData_t _sensor;
    uint8_t _address;
    Adafruit_CCS811 _ccs811;
};

#endif
