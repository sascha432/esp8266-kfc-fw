/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_BATTERY

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include <ReadADC.h>
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
    enum class BatteryType {
        LEVEL,
        CHARGING
    };

    Sensor_Battery(const JsonString &name);
    virtual ~Sensor_Battery();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override {
        return MQTTSensorSensorType::BATTERY;
    }
    virtual bool getSensorData(String &name, StringVector &values) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual void configurationSaved(Form *form) override;

    virtual void reconfigure(PGM_P source) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

    static float readSensor();

private:
    String _getId(BatteryType type = BatteryType::LEVEL);
    String _getTopic(BatteryType type = BatteryType::LEVEL);

    float _readSensor();
    bool _isCharging() const;

    JsonString _name;
    String _topic;
    Config_Sensor::battery_t _config;
    EventScheduler::Timer _timer;
    ReadADC _adc;
};

#endif
