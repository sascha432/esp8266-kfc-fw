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
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, F("temp"), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::TEMPERATURE));
            discovery->addUnitOfMeasurement(FSPGM(_degreeC, "\u00b0C"));
            break;
        case 1:
            discovery->create(this, F("temp2"), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::TEMPERATURE2));
            discovery->addUnitOfMeasurement(FSPGM(_degreeC));
            break;
        case 2:
            discovery->create(this, FSPGM(vcc), format);
            discovery->addStateTopic(_getMetricsTopics(TopicType::VCC));
            discovery->addUnitOfMeasurement('V');
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
    obj->add(JJ(id), F("dimmer_ntc_temp"));
    obj->add(JJ(state), _metrics.hasTemp());
    obj->add(JJ(value), JsonNumber(_metrics.getTemp(), 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_int_temp"));
    obj->add(JJ(state), _metrics.hasTemp2());
    obj->add(JJ(value), JsonNumber(_metrics.getTemp2(), 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_vcc"));
    obj->add(JJ(state), _metrics.hasVCC());
    obj->add(JJ(value), JsonNumber(_metrics.getVCC(), 3));

    obj = &array.addObject(3);
    obj->add(JJ(id), F("dimmer_frequency"));
    obj->add(JJ(state), _metrics.hasFrequency());
    obj->add(JJ(value), JsonNumber(_metrics.getFrequency(), 2));
}

void Sensor_DimmerMetrics::_createWebUI(WebUI &webUI, WebUIRow **row)
{
    (*row)->addBadgeSensor(F("dimmer_vcc"), _name, 'V');
    (*row)->addBadgeSensor(F("dimmer_frequency"), F("Frequency"), FSPGM(Hz));
    (*row)->addBadgeSensor(F("dimmer_int_temp"), F("ATmega"), FSPGM(_degreeC));
    (*row)->addBadgeSensor(F("dimmer_ntc_temp"), F("NTC"), FSPGM(_degreeC));
    _webUIinitialized = true;
}

void Sensor_DimmerMetrics::createWebUI(WebUI &webUI, WebUIRow **row)
{
    if (!_webUIinitialized) {
        _createWebUI(webUI, row);
    }
}

void Sensor_DimmerMetrics::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        auto _qos = MQTTClient::getDefaultQos();
        client->publish(_getMetricsTopics(TopicType::TEMPERATURE2), _qos, true, String(_metrics.getTemp2(), 2));
        client->publish(_getMetricsTopics(TopicType::TEMPERATURE), _qos, true, String(_metrics.getTemp(), 2));
        client->publish(_getMetricsTopics(TopicType::VCC), _qos, true, String(_metrics.getVCC(), 3));
        client->publish(_getMetricsTopics(TopicType::FREQUENCY), _qos, true, String(_metrics.getFrequency(), 2));
    }
}

void Sensor_DimmerMetrics::getStatus(PrintHtmlEntitiesString &output)
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

DimmerMetrics &Sensor_DimmerMetrics::_updateMetrics(const dimmer_metrics_t &metrics)
{
    _metrics = metrics;
    return _metrics;
}

#endif
