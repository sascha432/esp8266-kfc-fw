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

Sensor_DimmerMetrics::MQTTAutoDiscoveryPtr Sensor_DimmerMetrics::nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num)
{
    if (num > 3) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, F("temp"), format);
            discovery->addStateTopic(_getMetricsTopics(0));
            discovery->addUnitOfMeasurement(F("\u00b0C"));
            break;
        case 1:
            discovery->create(this, F("temp2"), format);
            discovery->addStateTopic(_getMetricsTopics(1));
            discovery->addUnitOfMeasurement(F("\u00b0C"));
            break;
        case 2:
            discovery->create(this, F("vcc"), format);
            discovery->addStateTopic(_getMetricsTopics(2));
            discovery->addUnitOfMeasurement('V');
            break;
        case 3:
            discovery->create(this, F("frequency"), format);
            discovery->addStateTopic(_getMetricsTopics(3));
            discovery->addUnitOfMeasurement(F("Hz"));
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
    (*row)->addBadgeSensor(F("dimmer_frequency"), F("Frequency"), F("Hz"));
    (*row)->addBadgeSensor(F("dimmer_int_temp"), F("ATmega"), F("\u00b0C"));
    (*row)->addBadgeSensor(F("dimmer_ntc_temp"), F("NTC"), F("\u00b0C"));
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
        client->publish(_getMetricsTopics(0), _qos, 1, String(_metrics.getTemp2(), 2));
        client->publish(_getMetricsTopics(1), _qos, 1, String(_metrics.getTemp(), 2));
        client->publish(_getMetricsTopics(2), _qos, 1, String(_metrics.getVCC(), 3));
        client->publish(_getMetricsTopics(3), _qos, 1, String(_metrics.getFrequency(), 2));
    }
}

void Sensor_DimmerMetrics::getStatus(PrintHtmlEntitiesString &output)
{
}

MQTTSensorSensorType Sensor_DimmerMetrics::getType() const
{
    return MQTTSensorSensorType::DIMMER_METRICS;
}

String Sensor_DimmerMetrics::_getMetricsTopics(uint8_t num) const
{
    String topic = MQTTClient::formatTopic(MQTTClient::NO_ENUM, F("/metrics/"));
    switch(num) {
        case 0:
            return topic + F("int_temp");
        case 1:
            return topic + F("ntc_temp");
        case 2:
            return topic + F("vcc");
        case 3:
            return topic + F("frequency");
        case 4:
            return topic + F("power");
    }
    return topic;
}

DimmerMetrics &Sensor_DimmerMetrics::_updateMetrics(const dimmer_metrics_t &metrics)
{
    _metrics = metrics;
    return _metrics;
}

#endif
