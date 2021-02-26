/**
 * Author: sascha_lammers@gmx.de
 */


#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include <JsonTools.h>
#include "Sensor_SystemMetrics.h"
#include <ArduinoJson.h>
#include "WebUIComponent.h"

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
    setUpdateRate(10);
    setMqttUpdateRate(60);
    setNextMqttUpdate(10);
}

Sensor_SystemMetrics::~Sensor_SystemMetrics()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

Sensor_SystemMetrics::AutoDiscoveryPtr Sensor_SystemMetrics::nextAutoDiscovery(FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    AutoDiscoveryPtr discovery = nullptr;
    switch(num) {
        case 0:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, FSPGM(uptime), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(FSPGM(seconds));
            discovery->addValueTemplate(FSPGM(uptime));
            break;
        case 1:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, FSPGM(heap), format);
            discovery->addStateTopic(_getTopic());
            discovery->addUnitOfMeasurement(FSPGM(bytes));
            discovery->addValueTemplate(FSPGM(heap));
            break;
        case 2:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, FSPGM(version), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(FSPGM(version));
            break;
        case 3:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("heap_frag"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("heap_frag"));
            break;
#if PING_MONITOR_SUPPORT
        case 4:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("ping_monitor_success"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("ping_monitor_success"));
            break;
        case 5:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("ping_monitor_failure"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("ping_monitor_failure"));
            break;
        case 6:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("ping_monitor_avg_resp_time"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("ping_monitor_avg_resp_time"));
            discovery->addUnitOfMeasurement(F("ms"));
            break;
        case 7:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("ping_monitor_rcvd_pkts"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("ping_monitor_rcvd_pkts"));
            break;
        case 8:
            discovery = __LDBG_new(AutoDiscovery);
            discovery->create(this, F("ping_monitor_lost_pkts"), format);
            discovery->addStateTopic(_getTopic());
            discovery->addValueTemplate(F("ping_monitor_lost_pkts"));
            break;
#endif
        default:
            return nullptr;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_SystemMetrics::getAutoDiscoveryCount() const
{
#if PING_MONITOR_SUPPORT
    return 9;
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

void Sensor_SystemMetrics::getValues(::JsonArray &array, bool timer)
{
    using ::JsonString;
    __LDBG_println();

    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(MetricsType::UPTIME));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _getUptime());

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(MetricsType::MEMORY));
    obj->add(JJ(state), true);
    obj->add(JJ(value), PrintString(F("%.3f KB<br>%u%%"), ESP.getFreeHeap() / 1024.0, ESP.getHeapFragmentation()));
}

String Sensor_SystemMetrics::_getUptime() const
{
    static constexpr auto kDaysPerYear = 365.25;
    static constexpr auto kDaysPerMonth = 365.25 / 12.0;
    static constexpr auto kSecondsPerDay = 86400;
    auto sep = F("<br>");

    PrintString uptimeStr;
    auto uptime = getSystemUptime();
    auto months = int(uptime / (kSecondsPerDay * kDaysPerMonth));
    auto years = int(uptime / (kSecondsPerDay * kDaysPerYear));
    if (years) {
        return formatTime2(sep, emptyString, false, 0, 0, 0, 0, 0, months % 12, years);
    }
    auto days = (uptime / kSecondsPerDay);
    if (months) {
        return formatTime2(sep, emptyString, false, 0, 0, 0, (days - (months * kDaysPerMonth)), 0, months);
    }
    auto weeks = (uptime / (kSecondsPerDay * 7)) % 4;
    if (weeks) {
        return formatTime2(sep, emptyString, false, 0, 0, 0, days % 7, weeks);
    }
    auto hours = uptime / 3600;
    if (days) {
        return formatTime2(sep, emptyString, false, 0, 0, hours % 24, days);
    }
    auto minutes = uptime / 60;
    if (hours) {
        return formatTime2(sep, emptyString, false, 0, minutes % 60, hours);
    }
    return formatTime2(sep, emptyString, false, uptime % 60, minutes);
}

void Sensor_SystemMetrics::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    using ::JsonString;
    __LDBG_println();
    // __DBG_printf("size=%u", (*row)->length());

    *row = &webUI.addRow();
    (*row)->addGroup(PrintString(F("System Metrics<div class=\"version d-md-inline\">%s</div>"), config.getFirmwareVersion().c_str()), false);

    *row = &webUI.addRow();
    (*row)->addSensor(_getId(MetricsType::UPTIME), F("Uptime"), F("")).add(JJ(hb), F("h2"));
    (*row)->addSensor(_getId(MetricsType::MEMORY), F("Free Memory"), F("")).add(JJ(hb), F("h2"));
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTTClient::formatTopic(F("sys"));
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
