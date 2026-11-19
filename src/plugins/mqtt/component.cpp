/**
 * Author: sascha_lammers@gmx.de
 */

#include "component.h"
#include "mqtt_client.h"
#include "mqtt_strings.h"

using namespace MQTT;

Component::Component(ComponentType type) :
    _type(type)
{
}

bool Component::hasClient() const
{
    return MQTT::Client::getClient() != nullptr;
}

bool Component::isConnected() const
{
    return MQTT::Client::safeIsConnected();
}

MQTT::Client *Component::getClient() const
{
    return MQTT::Client::getClient();
}

MQTT::Client &Component::client() const
{
    return *MQTT::Client::getClient();
}

bool Component::subscribe(const String &topic, QosType qos)
{
    auto client = MQTT::Client::getClient();
    return client && client->subscribe(const_cast<Component *>(this), topic, qos);
}

bool Component::unsubscribe(const String &topic)
{
    auto client = MQTT::Client::getClient();
    return client && client->unsubscribe(const_cast<Component *>(this), topic);
}

bool Component::publish(const String &topic, bool retain, const String &payload, QosType qos)
{
    auto client = MQTT::Client::getClient();
    return client && client->publish(topic, retain, payload, qos);
}

NameType Component::getNameByType(ComponentType type)
{
    switch(type) {
        case ComponentType::LIGHT:
            return FSPGM(mqtt_component_light);
        case ComponentType::SENSOR:
            return FSPGM(mqtt_component_sensor);
        case ComponentType::BINARY_SENSOR:
            return FSPGM(mqtt_component_binary_sensor);
        case ComponentType::DEVICE_AUTOMATION:
            return FSPGM(mqtt_component_device_automation);
        case ComponentType::FAN:
            return FSPGM(mqtt_component_fan);
        case ComponentType::BUTTON:
            return F("button");
        case ComponentType::SWITCH:
        default:
            break;
    }
    return FSPGM(mqtt_component_switch);
}
