/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

class Sensor_SystemMetrics : public MQTTSensor {
public:
    Sensor_SystemMetrics();
    virtual ~Sensor_SystemMetrics();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(::JsonArray &json, bool timer) override;
    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) override;
    virtual void getStatus(Print &output) override;

    virtual MQTTSensorSensorType getType() const override {
        return MQTTSensorSensorType::SYSTEM_METRICS;
    }

private:
    enum class MetricsType {
        UPTIME,
        MEMORY,
    };

    String _getTopic() const;
    String _getUptime() const;

    void _getMetricsJson(Print &json) const;
    const __FlashStringHelper *_getId(MetricsType type) const {
        switch(type) {
            case MetricsType::UPTIME:
                return F("metricsuptime");
            case MetricsType::MEMORY:
                return F("metricsmem");
        }
        return F("kfcfwmetrics");
    }
};

#endif
