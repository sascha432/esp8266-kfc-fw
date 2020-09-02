/**
 * Author: sascha_lammers@gmx.de
 */


#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include <JsonTools.h>
#include "Sensor_SystemMetrics.h"
#include <ArduinoJson.h>

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
        case 3:
            discovery->create(this, F("heap_frag"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("heap_frag"));
            break;
#if PING_MONITOR_SUPPORT
        case 4:
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
    return 5;
#else
    return 4;
#endif
}

void Sensor_SystemMetrics::getStatus(Print &output)
{
    output.printf_P(PSTR("System Metrics" HTML_S(br)));
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
#if 0
    __DBG_print("before");
    {
    JsonUnnamedObject obj;

    obj.add(FSPGM(uptime), getSystemUptime());
    obj.add(FSPGM(heap), (int)ESP.getFreeHeap());
    obj.add(F("heap_frag"), (int)ESP.getHeapFragmentation());
    obj.add(FSPGM(version), FPSTR(config.getShortFirmwareVersion_P()));

#if PING_MONITOR_SUPPORT
    PingMonitorTask::addToJson(obj);
#endif

    __DBG_print("after");

    obj.printTo(json);

    __DBG_print("after printto");


    }
    __DBG_print("return");
#else

#if PING_MONITOR_SUPPORT
    DynamicJsonDocument doc(316);
#else
    DynamicJsonDocument doc(128);
#endif

    doc[FSPGM(uptime)] = getSystemUptime();
    doc[FSPGM(heap)] = (int)ESP.getFreeHeap();
    doc[F("heap_frag")] = ESP.getHeapFragmentation();
    doc[FSPGM(version)] = FPSTR(config.getShortFirmwareVersion_P());

#if PING_MONITOR_SUPPORT
    PingMonitorTask::addToJson(doc);
#endif

    serializeJson(doc, json);
#endif
}

#endif
