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
#include "Sensor_SystemMetrics.h"
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
#include "Sensor_DimmerMetrics.h"

#ifndef IOT_SENSOR_NAMES_LM75A
#define IOT_SENSOR_NAMES_LM75A                  "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_BME280
#define IOT_SENSOR_NAMES_BME280                 "BME280"
#endif

#ifndef IOT_SENSOR_NAMES_DHTxx
#define IOT_SENSOR_NAMES_DHTxx                  "DHT11"
#endif

#ifndef IOT_SENSOR_NAMES_BME680
#define IOT_SENSOR_NAMES_BME680                 "BME680"
#endif

#ifndef IOT_SENSOR_NAMES_CCS811
#define IOT_SENSOR_NAMES_CCS811                 "CCS811"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8012
#define IOT_SENSOR_NAMES_HLW8012                "HLW8012"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8032
#define IOT_SENSOR_NAMES_HLW8032                "HLW8032"
#endif

#ifndef IOT_SENSOR_NAMES_BATTERY
#define IOT_SENSOR_NAMES_BATTERY                "Battery"
#endif

#ifndef IOT_SENSOR_NAMES_DS3231
#define IOT_SENSOR_NAMES_DS3231                 "DS3231 Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_INA219
#define IOT_SENSOR_NAMES_INA219                 "INA219 Voltage"
#endif

#ifndef IOT_SENSOR_NAMES_DIMMER_METRICS
#if IOT_ATOMIC_SUN_V2
#define IOT_SENSOR_NAMES_DIMMER_METRICS         "Atomic Sun VCC"
#else
#define IOT_SENSOR_NAMES_DIMMER_METRICS         "Dimmer VCC"
#endif
#endif

class SensorPlugin : public PluginComponent {
public:
    typedef std::vector<MQTTSensor *> SensorVector;

// WebUI
public:
    virtual void createWebUI(WebUIRoot &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// PluginComponent
public:
    SensorPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
    virtual void createMenu() override;

    static void timerEvent(Event::CallbackTimerPtr timer);

    static SensorVector &getSensors();
    static size_t getSensorCount();

    static SensorPlugin &getInstance();

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

private:
    bool _hasConfigureForm() const;
    void _timerEvent();

    SensorVector _sensors;
    Event::Timer _timer;

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
