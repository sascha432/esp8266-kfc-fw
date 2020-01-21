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
#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1           68.0
#endif
#ifndef IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2
#define IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2           300.0
#endif

// pin for charging detection
#ifndef IOT_SENSOR_BATTERY_CHARGE_DETECTION
#define IOT_SENSOR_BATTERY_CHARGE_DETECTION             5
#endif

// external function for charge detection
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION == -1
extern bool Sensor_Battery_charging_detection();
#endif

class Sensor_Battery : public MQTTSensor {
public:
    typedef enum {
        LEVEL,
        STATE
    } BatteryIdEnum_t;

    typedef struct {
        uint8_t adcReadCount;
        uint32_t adcSum;
    } SensorDataEx_t;

    Sensor_Battery(const JsonString &name);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual SensorEnumType_t getType() const override {
        return BATTERY;
    }

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form);

    virtual void reconfigure() override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, AtModeArgs &args) override;
#endif

    static float readSensor(SensorDataEx_t *data = nullptr);

private:
    String _getId(BatteryIdEnum_t type = LEVEL);
    String _getTopic(BatteryIdEnum_t type = LEVEL);

    float _readSensor(SensorDataEx_t *data = nullptr);
    bool _isCharging() const;

    JsonString _name;
    String _topic;
    float _calibration;
    EventScheduler::Timer _timer;
};

#endif
