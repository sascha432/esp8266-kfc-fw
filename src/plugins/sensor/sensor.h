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
#include "Sensor_Motion.h"
#include "Sensor_AmbientLight.h"

#ifndef IOT_SENSOR_NAMES_LM75A
#define IOT_SENSOR_NAMES_LM75A                  "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_2
#define IOT_SENSOR_NAMES_LM75A_2                "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_3
#define IOT_SENSOR_NAMES_LM75A_3                "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_4
#define IOT_SENSOR_NAMES_LM75A_4                "LM75A Temperature"
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
#define IOT_SENSOR_NAMES_BATTERY                "Battery Voltage"
#endif

#ifndef IOT_SENSOR_NAMES_MOTION_SENSOR
#define IOT_SENSOR_NAMES_MOTION_SENSOR          "Motion Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR
#define IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR   "Ambient Light Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR2
#define IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR2  "Illuminance Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_DS3231
#define IOT_SENSOR_NAMES_DS3231                 "DS3231 Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_INA219
#define IOT_SENSOR_NAMES_INA219                 "INA219 Voltage"
#endif

#ifndef IOT_SENSOR_NAMES_INA219_FORM_TITLE
#define IOT_SENSOR_NAMES_INA219_FORM_TITLE      "INA219 Voltage And Current Sensor"
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
    using SensorType = MQTT::SensorType;
    using SensorVector = std::vector<MQTT::SensorPtr>;
    using AddCustomSensorCallback = std::function<void(WebUINS::Root &webUI, SensorType nextType)>;

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// PluginComponent
public:
    SensorPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
    virtual void createMenu() override;

    static void timerEvent(Event::CallbackTimerPtr timer);

    static SensorVector &getSensors()
    {
        return getInstance()._sensors;
    }

    static size_t getSensorCount()
    {
        return getInstance()._sensors.size();
    }

    static SensorVector::iterator begin()
    {
        return getInstance()._sensors.begin();
    }

    static SensorVector::iterator end()
    {
        return getInstance()._sensors.end();
    }

    static SensorPlugin &getInstance();

    // the callback is invoked before a sensor is added
    // before the title row, the first Sensor is SensorType::MIN
    // the last type is SensorType::MAX for appending a new sensor at the end
    void setAddCustomSensorsCallback(AddCustomSensorCallback callback) {
        _addCustomSensors = callback;
    }
    // to use multiple callbacks, get the previous callback and call it from the new callback function
    // a copy, std:swap() or std::move() must be used
    AddCustomSensorCallback &getAddCustomSensorsCallback() {
        return _addCustomSensors;
    }


#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

protected:
    template<class _Sensor, typename... _Args>
    void addSensor(_Args &&... args) {
        auto sensor = new _Sensor(std::forward<_Args>(args)...);
        _sensors.push_back(sensor);
    }

private:
    bool _hasConfigureForm() const;
    void _timerEvent();

    size_t _count() const {
        return std::count_if(_sensors.begin(), _sensors.end(), [](const MQTT::SensorPtr sensor) {
            return (sensor->getType() != SensorType::SYSTEM_METRICS && sensor->getType() != SensorType::DIMMER_METRICS);
        });
    }

    void _sortSensors();

    SensorVector _sensors;
    Event::Timer _timer;
    AddCustomSensorCallback _addCustomSensors;
};
