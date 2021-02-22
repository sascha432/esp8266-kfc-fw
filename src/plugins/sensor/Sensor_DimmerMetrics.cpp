/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2 || IOT_DIMMER_MODULE

#include "Sensor_DimmerMetrics.h"
#include "../src/plugins/dimmer_module/firmware_protocol.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

Sensor_DimmerMetrics::Sensor_DimmerMetrics(const String &name) : MQTTSensor(), _name(name), _webUIinitialized(false)
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_DimmerMetrics::~Sensor_DimmerMetrics()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

Sensor_DimmerMetrics::MQTTAutoDiscoveryPtr Sensor_DimmerMetrics::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, F("temp"), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::TEMPERATURE));
            discovery->addUnitOfMeasurement(FSPGM(degree_Celsius_unicode));
            discovery->addDeviceClass(F("temperature"));
            break;
        case 1:
            discovery->create(this, F("temp2"), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::TEMPERATURE2));
            discovery->addUnitOfMeasurement(FSPGM(degree_Celsius_unicode));
            discovery->addDeviceClass(F("temperature"));
            break;
        case 2:
            discovery->create(this, FSPGM(vcc), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::VCC));
            discovery->addUnitOfMeasurement('V');
            discovery->addDeviceClass(F("voltage"));
            break;
        case 3:
            discovery->create(this, FSPGM(frequency), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::FREQUENCY));
            discovery->addUnitOfMeasurement(FSPGM(Hz, "Hz"));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_DimmerMetrics::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_DimmerMetrics::getValues(JsonArray &array, bool timer)
{
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), F("ntc_temp"));
    obj->add(JJ(state), !isnan(_metrics.metrics.ntc_temp));
    obj->add(JJ(value), JsonNumber(_metrics.metrics.ntc_temp, 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("int_temp"));
    obj->add(JJ(state), !isnan(_metrics.metrics.int_temp));
    obj->add(JJ(value), JsonNumber(_metrics.metrics.int_temp));

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(vcc));
    obj->add(JJ(state), _metrics.metrics.vcc != 0);
    obj->add(JJ(value), JsonNumber(_metrics.metrics.vcc / 1000.0, 3));

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(frequency));
    obj->add(JJ(state), !isnan(_metrics.metrics.frequency) && _metrics.metrics.frequency != 0);
    obj->add(JJ(value), JsonNumber(_metrics.metrics.frequency, 2));
}

void Sensor_DimmerMetrics::_createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    (*row)->addBadgeSensor(FSPGM(vcc), _name, 'V');
    (*row)->addBadgeSensor(FSPGM(frequency), F("Frequency"), FSPGM(Hz));
    (*row)->addBadgeSensor(F("int_temp"), F("ATmega"), FSPGM(degree_Celsius_html));
    (*row)->addBadgeSensor(F("ntc_temp"), F("NTC"), FSPGM(degree_Celsius_html));
    _webUIinitialized = true;
}

void Sensor_DimmerMetrics::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    if (!_webUIinitialized) {
        _createWebUI(webUI, row);
    }
}

void Sensor_DimmerMetrics::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(_getMetricsTopics(TopicType::TEMPERATURE2), true, String(_metrics.metrics.int_temp));
        client->publish(_getMetricsTopics(TopicType::TEMPERATURE), true, String(_metrics.metrics.ntc_temp, 2));
        client->publish(_getMetricsTopics(TopicType::VCC), true, String(_metrics.metrics.vcc, 3));
        client->publish(_getMetricsTopics(TopicType::FREQUENCY), true, String(_metrics.metrics.frequency, 2));
    }
}

void Sensor_DimmerMetrics::getStatus(Print &output)
{
}

MQTTSensorSensorType Sensor_DimmerMetrics::getType() const
{
    return MQTTSensorSensorType::DIMMER_METRICS;
}


String Sensor_DimmerMetrics::_getMetricsTopics(TopicType num) const
{
    String topic = MQTTClient::formatTopic(F("metrics/"), nullptr);
    switch(num) {
        case TopicType::TEMPERATURE2:
            return topic + F("int_temp");
        case TopicType::TEMPERATURE:
            return topic + F("ntc_temp");
        case TopicType::VCC:
            return topic + FSPGM(vcc);
        case TopicType::FREQUENCY:
            return topic + FSPGM(frequency);
    }
    return topic;
}

Sensor_DimmerMetrics::MetricsType &Sensor_DimmerMetrics::_updateMetrics(const MetricsType &metrics)
{
    _metrics = metrics;
    return _metrics;
}

#endif
