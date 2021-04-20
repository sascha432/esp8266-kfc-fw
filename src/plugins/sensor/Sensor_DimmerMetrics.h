/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_ATOMIC_SUN_V2 || IOT_DIMMER_MODULE

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

namespace Dimmer {
    class Plugin;
    class Base;
}

class Sensor_DimmerMetrics : public MQTT::Sensor {
public:
    using MetricsType = Dimmer::MetricsType;

    Sensor_DimmerMetrics(const String &name);
    virtual ~Sensor_DimmerMetrics();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

private:
    friend Dimmer::Plugin;
    friend Dimmer::Base;

    String _getMetricsTopics() const;
    MetricsType &_updateMetrics(const MetricsType &metrics);
    void _createWebUI(WebUINS::Root &webUI);

    String _name;
    MetricsType _metrics;
    bool _webUIinitialized;
};

inline Sensor_DimmerMetrics::Sensor_DimmerMetrics(const String &name) :
    MQTT::Sensor(SensorType::DIMMER_METRICS),
    _name(name),
    _webUIinitialized(false)
{
    REGISTER_SENSOR_CLIENT(this);
}

inline Sensor_DimmerMetrics::~Sensor_DimmerMetrics()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

inline uint8_t Sensor_DimmerMetrics::getAutoDiscoveryCount() const
{
    return 4;
}

inline void Sensor_DimmerMetrics::createWebUI(WebUINS::Root &webUI)
{
    if (!_webUIinitialized) {
        _createWebUI(webUI);
    }
}

inline void Sensor_DimmerMetrics::getStatus(Print &output)
{
}

inline String Sensor_DimmerMetrics::_getMetricsTopics() const
{
    return MQTTClient::formatTopic(F("metrics"));
}

inline Sensor_DimmerMetrics::MetricsType &Sensor_DimmerMetrics::_updateMetrics(const MetricsType &metrics)
{
    _metrics = metrics;
    return _metrics;
}


#endif
