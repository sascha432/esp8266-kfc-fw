/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <EventScheduler.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

#ifndef IOT_SENSOR_NAMES_LM75A
#define IOT_SENSOR_NAMES_LM75A      "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_BME280
#define IOT_SENSOR_NAMES_BME280     "BME280"
#endif

#ifndef IOT_SENSOR_NAMES_BME680
#define IOT_SENSOR_NAMES_BME680     "BME680"
#endif

#ifndef IOT_SENSOR_NAMES_CCS811
#define IOT_SENSOR_NAMES_CCS811     "CCS811"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8012
#define IOT_SENSOR_NAMES_HLW8012    "HLW8012"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8032
#define IOT_SENSOR_NAMES_HLW8032    "HLW8032"
#endif

#ifndef IOT_SENSOR_NAMES_BATTERY
#define IOT_SENSOR_NAMES_BATTERY    "Battery"
#endif

#ifndef IOT_SENSOR_NAMES_DS3231
#define IOT_SENSOR_NAMES_DS3231     "DS3231 Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_INA219
#define IOT_SENSOR_NAMES_INA219     "INA219 Voltage"
#endif

class SensorPlugin : public PluginComponent, public WebUIInterface {
public:
    typedef std::vector<MQTTSensor *> SensorVector;

// WebUIInterface
public:
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// PluginComponent
public:
    SensorPlugin() : _timer(nullptr) {
        REGISTER_PLUGIN(this, "SensorPlugin");
    }

    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)110;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    virtual PGM_P getConfigureForm() const override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override;

    static void timerEvent(EventScheduler::TimerPtr timer);

    static SensorVector &getSensors();
    static size_t getSensorCount();

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }

    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif

private:
    bool _hasConfigureForm() const;
    void _timerEvent();

    SensorVector _sensors;
    EventScheduler::TimerPtr _timer;
};

#endif

