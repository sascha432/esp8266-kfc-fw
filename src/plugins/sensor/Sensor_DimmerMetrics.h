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
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;

private:
    friend Dimmer::Plugin;
    friend Dimmer::Base;

    String _getMetricsTopics() const {
        return MQTTClient::formatTopic(F("metrics"));
    }
    MetricsType &_updateMetrics(const MetricsType &metrics);
    void _createWebUI(WebUIRoot &webUI, WebUIRow **row);

    String _name;
    MetricsType _metrics;
    bool _webUIinitialized;
};

#endif
