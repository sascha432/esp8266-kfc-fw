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

#ifndef IOT_SENSOR_CCS811_RENDER_TYPE
#define IOT_SENSOR_CCS811_RENDER_TYPE WebUINS::SensorRenderType::COLUMN
#endif

#ifndef IOT_SENSOR_CCS811_RENDER_HEIGHT
// #define IOT_SENSOR_CCS811_RENDER_HEIGHT F("15rem")
#endif

class Sensor_CCS811 : public MQTT::Sensor {
public:
    struct SensorData {
        int8_t available;   // -1 init, 0 N/A, 1 available
        uint8_t errors;
        uint16_t eCO2;      // ppm
        uint16_t TVOC;      // ppb
        SensorData() : available(-1), errors(0), eCO2(0), TVOC(0) {}
    };

    const uint8_t DEFAULT_UPDATE_RATE = MQTT::Sensor::DEFAULT_UPDATE_RATE;

public:
    Sensor_CCS811(const String &name, uint8_t address = CCS811_ADDRESS);
    virtual ~Sensor_CCS811();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

private:
    String _getId(const __FlashStringHelper *type = nullptr);
    String _getTopic(const __FlashStringHelper *type = nullptr);
    SensorData &_readSensor();

private:
    String _name;
    SensorData _sensor;
    uint8_t _address;
    Adafruit_CCS811 _ccs811;
};

inline uint8_t Sensor_CCS811::getAutoDiscoveryCount() const
{
    return 2;
}

inline String Sensor_CCS811::_getTopic(const __FlashStringHelper *type)
{
    return MQTTClient::formatTopic(_getId(type));
}

#endif
