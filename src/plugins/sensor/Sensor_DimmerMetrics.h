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
    Sensor_DimmerMetrics(const String &name);

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;

private:
    friend class AtomicSunPlugin;
    friend class DimmerModulePlugin;
    friend class Dimmer_Base;

    String _getMetricsTopics(uint8_t num) const;
    DimmerMetrics &_updateMetrics(const dimmer_metrics_t &metrics);
    void _createWebUI(WebUI &webUI, WebUIRow **row);

    // String _getId(const __FlashStringHelper *type = nullptr);
    String _name;
    DimmerMetrics _metrics;
    bool _webUIinitialized;
};

#endif
