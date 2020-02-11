/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR && IOT_SENSOR_HAVE_BME680

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include <Adafruit_BME680.h>

class Sensor_CCS811;

class Sensor_BME680 : public MQTTSensor {
public:
    typedef struct {
        float temperature;  // °C
        float humidity;     // %
        float pressure;     // hPa
        uint32_t gas;
    } SensorData_t;

    Sensor_BME680(const String &name, uint8_t address = 0x77);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType Sensor_LM75A::getType() const override;
}

private:
    friend Sensor_CCS811;

    String _getId(const __FlashStringHelper *type = nullptr);
    SensorData_t _readSensor();

    String _name;
    uint8_t _address;
};

#endif
