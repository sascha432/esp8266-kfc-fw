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

class Sensor_DimmerMetrics : public MQTTSensor {
public:
    using MetricsType = Dimmer::MetricsType;

    Sensor_DimmerMetrics(const String &name);
    virtual ~Sensor_DimmerMetrics();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;
    virtual MQTTSensorSensorType getType() const override;

private:
    friend class AtomicSunPlugin;
    friend class DimmerModulePlugin;
    friend class Dimmer_Base;

    enum class TopicType : uint8_t {
        TEMPERATURE,
        TEMPERATURE2,
        VCC,
        FREQUENCY
    };

    String _getMetricsTopics(TopicType num) const;
    MetricsType &_updateMetrics(const MetricsType &metrics);
    void _createWebUI(WebUIRoot &webUI, WebUIRow **row);

    // String _getId(const __FlashStringHelper *type = nullptr);
    String _name;
    MetricsType _metrics;
    bool _webUIinitialized;
};

#endif
