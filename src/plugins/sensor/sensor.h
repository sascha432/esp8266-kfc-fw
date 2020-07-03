/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include <EventScheduler.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include "Sensor_LM75A.h"
#include "Sensor_BME280.h"
#include "Sensor_BME680.h"
#include "Sensor_CCS811.h"
#include "Sensor_HLW80xx.h"
#include "Sensor_HLW8012.h"
#include "Sensor_HLW8032.h"
#include "Sensor_Battery.h"
#include "Sensor_DS3231.h"
#include "Sensor_INA219.h"
#include "Sensor_DHTxx.h"

#ifndef IOT_SENSOR_NAMES_LM75A
#define IOT_SENSOR_NAMES_LM75A      "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_BME280
#define IOT_SENSOR_NAMES_BME280     "BME280"
#endif

#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx      "DHT11"
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
    SensorPlugin() {
        REGISTER_PLUGIN(this);
    }

    virtual PGM_P getName() const;
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Sensors");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)110;
    }
#if ENABLE_DEEP_SLEEP
    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }
#endif

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    virtual PGM_P getConfigureForm() const override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

    static void timerEvent(EventScheduler::TimerPtr timer);

    static SensorVector &getSensors();
    static size_t getSensorCount();

    static SensorPlugin &getInstance();

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

private:
    bool _hasConfigureForm() const;
    void _timerEvent();

    SensorVector _sensors;
    EventScheduler::Timer _timer;

public:
    template <class T>
    static uint8_t for_each(T *self, std::function<void(T &sensor)> callback) {
        uint8_t count = 0;
        for(auto sensor: SensorPlugin::getSensors()) {
            if (self->getType() == sensor->getType()) {
                callback(*reinterpret_cast<T *>(sensor));
                count++;
            }
        }
        return count;
    }

    template <class T, class R>
    static R for_each(T *self, R defaultValue, std::function<R(T &sensor)> callback) {
        for(auto sensor: SensorPlugin::getSensors()) {
            if (self->getType() == sensor->getType()) {
                return callback(*reinterpret_cast<T *>(sensor));
            }
        }
        return defaultValue;
    }

    template <class T>
    static uint8_t for_each(T *self, std::function<bool(MQTTSensor &sensor, T &self)> compareCallback, std::function<void(T &sensor)> callback) {
        uint8_t count = 0;
        for(auto sensor: SensorPlugin::getSensors()) {
            if (compareCallback(*sensor, *self)) {
                callback(*reinterpret_cast<T *>(sensor));
                count++;
            }
        }
        return count;
    }
};
