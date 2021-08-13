/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include "Sensor_SystemMetrics.h"
#include "WebUIComponent.h"
#include "../src/plugins/mqtt/mqtt_json.h"


#if PING_MONITOR_SUPPORT
#include "../src/plugins/ping_monitor/ping_monitor.h"
#endif

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AUTO_STRING_DEF(uptime, "uptime")
AUTO_STRING_DEF(heap, "heap")
AUTO_STRING_DEF(bytes, "bytes")
AUTO_STRING_DEF(version, "version")

Sensor_SystemMetrics::Sensor_SystemMetrics() : MQTT::Sensor(MQTT::SensorType::SYSTEM_METRICS)
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

MQTT::AutoDiscovery::EntityPtr Sensor_SystemMetrics::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    MQTT::AutoDiscovery::EntityPtr discovery = new MQTT::AutoDiscovery::Entity();
    #if MQTT_AUTO_DISCOVERY_USE_NAME
        String name = KFCConfigurationClasses::System::Device::getName();
        name += ' ';
    #endif
    switch(num) {
        case 0:
            if (discovery->create(this, FSPGM(uptime), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addUnitOfMeasurement(FSPGM(seconds));
                discovery->addValueTemplate(FSPGM(uptime));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(name + F("System Uptime (seconds)"));
                #endif
            }
            break;
        case 1:
            if (discovery->create(this, F("uptime_hr"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("uptime_hr"));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(name + F("System Uptime"));
                #endif
            }
            break;
        case 2:
            if (discovery->create(this, FSPGM(heap), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addUnitOfMeasurement(FSPGM(bytes));
                discovery->addValueTemplate(FSPGM(heap));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(name + F("Free Heap"));
                #endif
            }
            break;
        case 3:
            if (discovery->create(this, FSPGM(version), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(FSPGM(version));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(name + F("Firmware Version"));
                #endif
            }
            break;
        case 4:
            if (discovery->create(this, F("heap_frag"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("heap_frag"));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(name + F("Heap Fragmentation"));
                #endif
            }
            break;
#if PING_MONITOR_SUPPORT
        case 5:
            if (discovery->create(this, F("ping_monitor_success"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("ping_monitor_success"));
            }
            break;
        case 6:
            if (discovery->create(this, F("ping_monitor_failure"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("ping_monitor_failure"));
            }
            break;
        case 7:
            if (discovery->create(this, F("ping_monitor_avg_resp_time"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("ping_monitor_avg_resp_time"));
                discovery->addUnitOfMeasurement(F("ms"));
            }
            break;
        case 8:
            if (discovery->create(this, F("ping_monitor_rcvd_pkts"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("ping_monitor_rcvd_pkts"));
            }
            break;
        case 9:
            if (discovery->create(this, F("ping_monitor_lost_pkts"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("ping_monitor_lost_pkts"));
            }
            break;
#endif
        default:
            __DBG_panic("getAutoDiscovery(%u) does not exist", num);
    }
    return discovery;
}

uint8_t Sensor_SystemMetrics::getAutoDiscoveryCount() const
{
#if PING_MONITOR_SUPPORT
    return 10;
#else
    return 5;
#endif
}

void Sensor_SystemMetrics::getStatus(Print &output)
{
    output.printf_P(PSTR("System Metrics" HTML_S(br)));
}

void Sensor_SystemMetrics::publishState()
{
    if (isConnected()) {
        publish(_getTopic(), true, _getMetricsJson());
    }
}

void Sensor_SystemMetrics::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        WebUINS::Values(_getId(MetricsType::UPTIME), _getUptime(), true),
        WebUINS::Values(_getId(MetricsType::MEMORY), PrintString(F("%.3f KB<br>%u%%"), ESP.getFreeHeap() / 1024.0, ESP.getHeapFragmentation()), true)
    );
}

String Sensor_SystemMetrics::_getUptime(const __FlashStringHelper *sep) const
{
    static constexpr auto kDaysPerYear = 365.25;
    static constexpr auto kDaysPerMonth = 365.25 / 12.0;
    static constexpr auto kSecondsPerDay = 86400;

    PrintString uptimeStr;
    auto uptime = getSystemUptime();
    auto months = int(uptime / (kSecondsPerDay * kDaysPerMonth));
    auto years = int(uptime / (kSecondsPerDay * kDaysPerYear));
    if (years) {
        return formatTimeShort(sep, emptyString, false, 0, 0, 0, 0, 0, months % 12, years);
    }
    auto days = (uptime / kSecondsPerDay);
    if (months) {
        return formatTimeShort(sep, emptyString, false, 0, 0, 0, (days - (months * kDaysPerMonth)), 0, months);
    }
    auto weeks = (uptime / (kSecondsPerDay * 7)) % 4;
    if (weeks) {
        return formatTimeShort(sep, emptyString, false, 0, 0, 0, days % 7, weeks);
    }
    auto hours = uptime / 3600;
    if (days) {
        return formatTimeShort(sep, emptyString, false, 0, 0, hours % 24, days);
    }
    auto minutes = uptime / 60;
    if (hours) {
        return formatTimeShort(sep, emptyString, false, 0, minutes % 60, hours);
    }
    return formatTimeShort(sep, emptyString, false, uptime % 60, minutes);
}

void Sensor_SystemMetrics::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(WebUINS::Group(PrintString(F("System Metrics<div class=\"version d-md-inline\">%s</div>"), config.getFirmwareVersion().c_str()), false)));

    WebUINS::Row row;
    row.append(WebUINS::Sensor(_getId(MetricsType::UPTIME), F("Uptime"), F("")).append(WebUINS::NamedString(J(heading_bottom), F("h2"))).setConfig(_renderConfig));
    row.append(WebUINS::Sensor(_getId(MetricsType::MEMORY), F("Free Memory"), F("")).append(WebUINS::NamedString(J(heading_bottom), F("h2"))).setConfig(_renderConfig));
    webUI.addRow(row);
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTT::Client::formatTopic(F("sys"));
}

String Sensor_SystemMetrics::_getMetricsJson() const
{
    using namespace MQTT::Json;
    UnnamedObject jsonObj(
        NamedUint32(FSPGM(uptime), getSystemUptime()),
        NamedStoredString(F("uptime_hr"), _getUptime(F("\n"))),
        NamedUint32(FSPGM(heap), ESP.getFreeHeap()),
        NamedShort(F("heap_frag"), ESP.getHeapFragmentation()),
        NamedString(FSPGM(version), FPSTR(config.getShortFirmwareVersion_P()))
    );

#if PING_MONITOR_SUPPORT
    PingMonitor::Task::addToJson(jsonObj);
#endif
    return jsonObj.toString();
}

const __FlashStringHelper *Sensor_SystemMetrics::_getId(MetricsType type) const
{
    switch(type) {
        case MetricsType::UPTIME:
            return F("metricsuptime");
        case MetricsType::MEMORY:
            return F("metricsmem");
    }
    return F("kfcfwmetrics");
}

#endif
