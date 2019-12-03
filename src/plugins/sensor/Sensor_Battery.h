/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR && IOT_SENSOR_HAVE_BATTERY

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

// convert ADC value to voltage
#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1           68.0
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2           300.0
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER              ((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) / IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)
#endif

// pin for charging detection
#ifndef IOT_SENSOR_BATTERY_CHARGE_DETECTION
#define IOT_SENSOR_BATTERY_CHARGE_DETECTION             5
#endif


class Sensor_Battery : public MQTTSensor {
public:
    Sensor_Battery(const JsonString &name);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual SensorEnumType_t getType() const override;

private:
    String _getId();
    float _readSensor();

    JsonString _name;
    String _topic;
};

#endif
