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

class Sensor_SystemMetrics : public MQTT::Sensor {
public:
    Sensor_SystemMetrics();
    virtual ~Sensor_SystemMetrics();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

private:
    enum class MetricsType {
        UPTIME,
        MEMORY,
    };

    String _getTopic() const;
    String _getUptime(const __FlashStringHelper *sep = FSPGM(html_br_, "<br>")) const;

    String _getMetricsJson() const;

    const __FlashStringHelper *_getId(MetricsType type) const;
};

#endif
