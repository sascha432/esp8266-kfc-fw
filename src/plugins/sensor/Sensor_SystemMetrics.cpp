/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_SYSTEM_METRICS

#include "Sensor_SystemMetrics.h"
#include "WebUIComponent.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#if ESP32
#include <sdkconfig.h>
#endif

#if PING_MONITOR_SUPPORT
#include "../src/plugins/ping_monitor/ping_monitor.h"
#endif

#include <save_crash.h>

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

enum class AutoDiscoveryENum {
    UPTIME_SECONDS = 0,
    UPTIME_HUMAN,
    HEAP,
    #if ESP32 && (CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM)
        PSRAM,
    #endif
    VERSION,
    #if ESP8266
        SAVE_CRASH,
        HEAP_FRAGMENTATION,
    #endif
    #if PING_MONITOR_SUPPORT
        PING_MOINOTOR_SUCCESS,
        PING_MOINOTOR_FAILED,
        PING_MOINOTOR_AVG_RESP_TIME,
        PING_MOINOTOR_RCVD_PACKETS,
        PING_MOINOTOR_LOST_PACKETS,
    #endif
    MAX
};

MQTT::AutoDiscovery::EntityPtr Sensor_SystemMetrics::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    MQTT::AutoDiscovery::EntityPtr discovery = new MQTT::AutoDiscovery::Entity();
    switch(static_cast<AutoDiscoveryENum>(num)) {
        case AutoDiscoveryENum::UPTIME_SECONDS:
            if (discovery->create(this, FSPGM(uptime), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addUnitOfMeasurement(FSPGM(seconds));
                discovery->addValueTemplate(FSPGM(uptime));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("System Uptime (seconds)")));
                #endif
            }
            break;
        case AutoDiscoveryENum::UPTIME_HUMAN:
            if (discovery->create(this, F("uptime_hr"), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(F("uptime_hr"));
                discovery->addIcon(F("mdi:hours-24"));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("System Uptime")));
                #endif
            }
            break;
        case AutoDiscoveryENum::HEAP:
            if (discovery->create(this, FSPGM(heap), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addUnitOfMeasurement(FSPGM(bytes));
                discovery->addValueTemplate(FSPGM(heap));
                discovery->addIcon(F("mdi:memory"));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Free Heap")));
                #endif
            }
            break;
        #if ESP32 && (CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM)
            case AutoDiscoveryENum::PSRAM:
                if (discovery->create(this, "psram", format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addUnitOfMeasurement(FSPGM(bytes));
                    discovery->addValueTemplate(F("psram"));
                    discovery->addIcon(F("mdi:memory"));
                    #if MQTT_AUTO_DISCOVERY_USE_NAME
                        discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Free PSRAM")));
                    #endif
                }
                break;
        #endif
        case AutoDiscoveryENum::VERSION:
            if (discovery->create(this, FSPGM(version), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addValueTemplate(FSPGM(version));
                discovery->addIcon(F("mdi:wrench"));
                #if MQTT_AUTO_DISCOVERY_USE_NAME
                    discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Firmware Version")));
                #endif
            }
            break;
        #if ESP8266
            case AutoDiscoveryENum::SAVE_CRASH:
                if (discovery->create(this, F("savecrash_cnt"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("savecrash_cnt"));
                    discovery->addIcon(F("mdi:math-log"));
                    #if MQTT_AUTO_DISCOVERY_USE_NAME
                        discovery->addName(MQTT::Client::getAutoDiscoveryName(F("SaveCrash Log Count")));
                    #endif
                }
                break;
            case AutoDiscoveryENum::HEAP_FRAGMENTATION:
                if (discovery->create(this, F("heap_frag"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("heap_frag"));
                    #if MQTT_AUTO_DISCOVERY_USE_NAME
                        discovery->addName(MQTT::Client::getAutoDiscoveryName(F("Heap Fragmentation")));
                    #endif
                }
                break;
        #endif
        #if PING_MONITOR_SUPPORT
            case AutoDiscoveryENum::PING_MOINOTOR_SUCCESS:
                if (discovery->create(this, F("success"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("success"));
                }
                break;
            case AutoDiscoveryENum::PING_MOINOTOR_FAILED:
                if (discovery->create(this, F("failure"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("failure"));
                }
                break;
            case AutoDiscoveryENum::PING_MOINOTOR_AVG_RESP_TIME:
                if (discovery->create(this, F("avg_resp_time"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("avg_resp_time"));
                    discovery->addUnitOfMeasurement(F("ms"));
                }
                break;
            case AutoDiscoveryENum::PING_MOINOTOR_RCVD_PACKETS:
                if (discovery->create(this, F("rcvd_pkts"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("rcvd_pkts"));
                }
                break;
            case AutoDiscoveryENum::PING_MOINOTOR_LOST_PACKETS:
                if (discovery->create(this, F("lost_pkts"), format)) {
                    discovery->addStateTopic(_getTopic());
                    discovery->addValueTemplate(F("lost_pkts"));
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
    return static_cast<uint8_t>(AutoDiscoveryENum::MAX);
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
        WebUINS::Values(_getId(MetricsType::UPTIME), _getUptime(), true)
        #if ESP32
            , WebUINS::Values(_getId(MetricsType::MEMORY), PrintString(F("%.2f KB"), ESP.getFreeHeap() / 1024.0), true)
            #if CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM
                , WebUINS::Values(_getId(MetricsType::PSRAM), PrintString(F("%.2f KB"), ESP.getFreePsram() / 1024.0), true)
            #endif
        #endif
        #if ESP8266
            , WebUINS::Values(_getId(MetricsType::MEMORY), PrintString(F("%.2f KB<br>%u%%"), ESP.getFreeHeap() / 1024.0, ESP.getHeapFragmentation()), true)
        #endif
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
    #if ESP32 && (CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM)
        row.append(WebUINS::Sensor(_getId(MetricsType::PSRAM), F("Free PSRam"), F("")).append(WebUINS::NamedString(J(heading_bottom), F("h2"))).setConfig(_renderConfig));
    #endif
    webUI.addRow(row);
}

String Sensor_SystemMetrics::_getTopic() const
{
    return MQTT::Client::formatTopic(F("sys"));
}

String Sensor_SystemMetrics::_getMetricsJson() const
{
    using namespace MQTT::Json;

    #if ESP8266
        auto fs = SaveCrash::createFlashStorage();
        auto saveCrashInfo = fs.getInfo();
    #endif

    UnnamedObject jsonObj(
        NamedUint32(FSPGM(uptime), getSystemUptime()),
        NamedStoredString(F("uptime_hr"), _getUptime(F("\n"))),
            NamedUint32(FSPGM(heap), ESP.getFreeHeap()),
        #if ESP32 && (CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM)
            NamedUint32(F("psram"), ESP.getFreePsram()),
        #endif
        #if ESP8266
            NamedShort(F("savecrash_cnt"), saveCrashInfo.numTraces()),
            NamedShort(F("heap_frag"), ESP.getHeapFragmentation()),
        #endif
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
        #if ESP32 && (CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM)
            case MetricsType::PSRAM:
                return F("metricspsram");
        #endif
    }
    return F("kfcfwmetrics");
}

#endif
