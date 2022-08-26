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

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#ifndef IOT_SENSOR_NAMES_LM75A
#    define IOT_SENSOR_NAMES_LM75A "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_2
#    define IOT_SENSOR_NAMES_LM75A_2 "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_3
#    define IOT_SENSOR_NAMES_LM75A_3 "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_LM75A_4
#    define IOT_SENSOR_NAMES_LM75A_4 "LM75A Temperature"
#endif

#ifndef IOT_SENSOR_NAMES_BME280
#    define IOT_SENSOR_NAMES_BME280 "BME280"
#endif

#ifndef IOT_SENSOR_NAMES_DHTxx
#    define IOT_SENSOR_NAMES_DHTxx "DHT11"
#endif

#ifndef IOT_SENSOR_NAMES_BME680
#    define IOT_SENSOR_NAMES_BME680 "BME680"
#endif

#ifndef IOT_SENSOR_NAMES_CCS811
#    define IOT_SENSOR_NAMES_CCS811 "CCS811"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8012
#    define IOT_SENSOR_NAMES_HLW8012 "HLW8012"
#endif

#ifndef IOT_SENSOR_NAMES_HLW8032
#    define IOT_SENSOR_NAMES_HLW8032 "HLW8032"
#endif

#ifndef IOT_SENSOR_NAMES_BATTERY
#    define IOT_SENSOR_NAMES_BATTERY "Battery Voltage"
#endif

#ifndef IOT_SENSOR_NAMES_MOTION_SENSOR
#    define IOT_SENSOR_NAMES_MOTION_SENSOR "Motion Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR
#    define IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR "Ambient Light Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_ILLUMINANCE_LIGHT_SENSOR
#    define IOT_SENSOR_NAMES_ILLUMINANCE_LIGHT_SENSOR "Illuminance Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_DS3231
#    define IOT_SENSOR_NAMES_DS3231 "DS3231 RTC Clock"
#endif

#ifndef IOT_SENSOR_NAMES_INA219
#    define IOT_SENSOR_NAMES_INA219 "INA219 Voltage"
#endif

#ifndef IOT_SENSOR_NAMES_INA219_FORM_TITLE
#    define IOT_SENSOR_NAMES_INA219_FORM_TITLE "INA219 Voltage And Current Sensor"
#endif

#ifndef IOT_SENSOR_NAMES_DIMMER_METRICS
#    if IOT_ATOMIC_SUN_V2
#        define IOT_SENSOR_NAMES_DIMMER_METRICS "Atomic Sun VCC"
#    else
#        define IOT_SENSOR_NAMES_DIMMER_METRICS "Dimmer VCC"
#    endif
#endif

class SensorPlugin : public PluginComponent {
public:
    using Plugins = KFCConfigurationClasses::PluginsType;
    using SensorType = MQTT::SensorType;
    template<SensorType _SensorType>
    using SensorClassType = MQTT::SensorClassType<_SensorType>;
    using SensorConfig = WebUINS::SensorConfig;
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

    static SensorVector &getSensors();

    template<typename _Ta>
    static _Ta *getSensor(SensorType type);

    template<typename _Ta>
    static _Ta *getSensor(MQTT::SensorPtr sensor);

    template<SensorType _Type>
    static typename SensorClassType<_Type>::type *getSensor() {
        return getSensor<typename SensorClassType<_Type>::type>(_Type);
    }

    static size_t getSensorCount();
    static SensorVector::iterator begin();
    static SensorVector::iterator end();

    static SensorPlugin &getInstance();

    // the callback is invoked before a sensor is added
    // before the title row, the first Sensor is SensorType::MIN
    // the last type is SensorType::MAX for appending a new sensor at the end
    void setAddCustomSensorsCallback(AddCustomSensorCallback callback);

    // to use multiple callbacks, get the previous callback and call it from the new callback function
    // a copy, std:swap() or std::move() must be used
    AddCustomSensorCallback &getAddCustomSensorsCallback();

    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED
            virtual void atModeHelpGenerator() override;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

protected:
    template<class _Sensor, typename... _Args>
    void addSensor(_Args ... args) {
        addSensor(new _Sensor(args...));
    }

public:
    void addSensor(MQTT::SensorPtr sensor, const SensorConfig &config = SensorConfig());

private:
    bool _hasConfigureForm() const;
    void _timerEvent();
    size_t _count() const;
    void _sortSensors();

    SensorVector _sensors;
    Event::Timer _timer;
    AddCustomSensorCallback _addCustomSensors;
};

inline SensorPlugin::SensorVector &SensorPlugin::getSensors()
{
    return getInstance()._sensors;
}

inline size_t SensorPlugin::getSensorCount()
{
    return getInstance()._sensors.size();
}

inline SensorPlugin::SensorVector::iterator SensorPlugin::begin()
{
    return getInstance()._sensors.begin();
}

inline SensorPlugin::SensorVector::iterator SensorPlugin::end()
{
    return getInstance()._sensors.end();
}

inline void SensorPlugin::addSensor(MQTT::SensorPtr sensor, const SensorConfig &config)
{
    sensor->setConfig(config);
    _sensors.push_back(sensor);
}

template<typename _Ta>
inline _Ta *SensorPlugin::getSensor(SensorType type)
{
    auto &sensors = SensorPlugin::getSensors();
    auto iterator = std::find_if(sensors.begin(), sensors.end(), [type](const MQTT::SensorPtr sensor) {
        return sensor->getType() == type;

    });
    if (iterator == sensors.end()) {
        return nullptr;
    }
    return reinterpret_cast<_Ta *>(*iterator);

}

template<typename _Ta>
inline _Ta *SensorPlugin::getSensor(MQTT::SensorPtr sensor)
{
    return reinterpret_cast<_Ta *>(sensor);
}

inline size_t SensorPlugin::_count() const
{
    return std::count_if(_sensors.begin(), _sensors.end(), [](const MQTT::SensorPtr sensor) {
        return (sensor->getType() != SensorType::SYSTEM_METRICS && sensor->getType() != SensorType::DIMMER_METRICS);
    });
}

inline void SensorPlugin::getValues(WebUINS::Events &array)
{
    for(const auto sensor: _sensors) {
        sensor->getValues(array, false);
    }
}

inline void SensorPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("setValue(%s)", id.c_str());
}

inline void SensorPlugin::setAddCustomSensorsCallback(AddCustomSensorCallback callback)
{
    _addCustomSensors = callback;
}

inline SensorPlugin::AddCustomSensorCallback &SensorPlugin::getAddCustomSensorsCallback()
{
    return _addCustomSensors;
}

inline void SensorPlugin::_sortSensors()
{
    std::sort(_sensors.begin(), _sensors.end(), [](const MQTT::SensorPtr a, const MQTT::SensorPtr b) {
         return b->getOrderId() >= a->getOrderId();
    });
}

inline void SensorPlugin::shutdown()
{
    _timer.remove();
    for(const auto sensor: _sensors) {
        __LDBG_printf("type=%u", sensor->getType());
        sensor->shutdown();
        delete sensor;
    }
    _sensors.clear();
}

inline void SensorPlugin::reconfigure(const String &source)
{
    _sortSensors();
    for(const auto sensor: _sensors) {
        sensor->reconfigure(source.c_str());
    }
}

inline void SensorPlugin::timerEvent(Event::CallbackTimerPtr timer)
{
    getInstance()._timerEvent();
}

inline bool SensorPlugin::_hasConfigureForm() const
{
    for(const auto sensor: _sensors) {
        if (sensor->hasForm()) {
            return true;
        }
    }
    return false;
}

#if DEBUG_IOT_SENSOR
#include <debug_helper_disable.h>
#endif
