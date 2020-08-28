/**
 * Author: sascha_lammers@gmx.de
 */


#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include <JsonTools.h>
#include "Sensor_SystemMetrics.h"

#if PING_MONITOR_SUPPORT
#include "../src/plugins/ping_monitor/ping_monitor.h"
#endif

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>


FLASH_STRING_GENERATOR_AUTO_INIT(
    AUTO_STRING_DEF(uptime, "uptime")
    AUTO_STRING_DEF(heap, "heap")
    AUTO_STRING_DEF(bytes, "bytes")
    AUTO_STRING_DEF(version, "version")
);

Sensor_SystemMetrics::Sensor_SystemMetrics() : MQTTSensor()
{
    REGISTER_SENSOR_CLIENT(this);
    setUpdateRate(3600); // not in use
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
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, FSPGM(uptime), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(FSPGM(seconds));
            discovery->addValueTemplate(FSPGM(uptime));
            break;
        case 1:
            discovery->create(this, FSPGM(heap), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(FSPGM(bytes));
            discovery->addValueTemplate(FSPGM(heap));
            break;
        case 2:
            discovery->create(this, FSPGM(version), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(FSPGM(version));
            break;
#if PING_MONITOR_SUPPORT
        case 3:
            discovery->create(this, FSPGM(ping_monitor), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(FSPGM(ping_monitor));
            break;
#endif
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_SystemMetrics::getAutoDiscoveryCount() const
{
#if PING_MONITOR_SUPPORT
    return 4;
#else
    return 3;
#endif
}

void Sensor_SystemMetrics::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        PrintString json;
        _getMetricsJson(json);
        client->publish(_getTopic(), true, json);
    }
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTTClient::formatTopic(FSPGM(sys));
}

void Sensor_SystemMetrics::_getMetricsJson(Print &json) const
{
    JsonUnnamedObject obj;

    obj.add(FSPGM(uptime), getSystemUptime());
    obj.add(FSPGM(heap), (int)ESP.getFreeHeap());
    obj.add(FSPGM(version), FPSTR(config.getShortFirmwareVersion_P()));

#if PING_MONITOR_SUPPORT
    PingMonitorTask::addToJson(obj);
#endif

    obj.printTo(json);
}

#endif
