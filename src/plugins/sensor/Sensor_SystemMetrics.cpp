/**
 * Author: sascha_lammers@gmx.de
 */


#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include "Sensor_SystemMetrics.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_SystemMetrics::Sensor_SystemMetrics() : MQTTSensor()
{
    REGISTER_SENSOR_CLIENT(this);
    setUpdateRate(3600);
    setMqttUpdateRate(60);
    setNextMqttUpdate(10);
}

Sensor_SystemMetrics::~Sensor_SystemMetrics()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_SystemMetrics::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, F("uptime"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(FSPGM(seconds));
            discovery->addValueTemplate(F("uptime"));
            break;
        case 1:
            discovery->create(this, F("heap"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(F("bytes"));
            discovery->addValueTemplate(F("heap"));
            break;
        case 2:
            discovery->create(this, F("version"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("version"));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_SystemMetrics::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_SystemMetrics::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        String json;
        _getMetricsJson(json);
        client->publish(_getTopic(), true, json);
    }
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTTClient::formatTopic(F("sys"));
}

void Sensor_SystemMetrics::_getMetricsJson(String &json) const
{
    json = PrintString(F("{\"uptime\":%u,\"heap\":%u,\"version\":\"%s\"}"), getSystemUptime(), ESP.getFreeHeap(), config.getShortFirmwareVersion_P());
}

#endif
