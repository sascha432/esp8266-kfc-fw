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
    String topic = _getTopic();
    switch(num) {
        case 0:
            discovery->create(this, F("uptime"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(FSPGM(seconds));
            discovery->addValueTemplate(F("uptime"));
            break;
        case 1:
            discovery->create(this, F("heap"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(F("bytes"));
            discovery->addValueTemplate(F("heap"));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_SystemMetrics::getAutoDiscoveryCount() const
{
    return 2;
}

void Sensor_SystemMetrics::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        auto _qos = MQTTClient::getDefaultQos();
        client->publish(_getTopic(), _qos, true, _getMetricsJson());
    }
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTTClient::formatTopic(F("sys"));
}

String Sensor_SystemMetrics::_getMetricsJson() const
{
    return PrintString(F("{\"uptime\":%u,\"heap\":%u}"), getSystemUptime(), ESP.getFreeHeap());
}

#endif
