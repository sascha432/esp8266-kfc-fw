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

class Sensor_CCS811 : public MQTTSensor {
public:
    typedef struct {
        bool available;
        uint16_t eCO2;      // ppm
        uint16_t TVOC;      // ppb
    } SensorData_t;

    const uint8_t DEFAULT_UPDATE_RATE = MQTTSensor::DEFAULT_UPDATE_RATE;

    Sensor_CCS811(const String &name, uint8_t address = CCS811_ADDRESS);

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;

private:
    String _getId(const __FlashStringHelper *type = nullptr);
    SensorData_t &_readSensor();

    String _name;
    SensorData_t _sensor;
    uint8_t _address;
    Adafruit_CCS811 _ccs811;
};

#endif
