/**
 * Author: sascha_lammers@gmx.de
 */

#include "mqtt_component.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

MQTTComponent::MQTTComponent(ComponentType type) : _type(type), _autoDiscoveryNum(0)
{
}

MQTTComponent::~MQTTComponent()
{
}

void MQTTComponent::onConnect(MQTTClient *client)
{
}

void MQTTComponent::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason)
{
}

void MQTTComponent::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
}

#if MQTT_AUTO_DISCOVERY

uint8_t MQTTComponent::rewindAutoDiscovery()
{
    uint8_t count;
    if ((count = getAutoDiscoveryCount()) != 0) {
        _autoDiscoveryNum = 0;
        return true;
    }
    return false;
}

#endif

MQTTComponent::NameType MQTTComponent::getNameByType(ComponentType type)
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
        case ComponentType::SWITCH:
        default:
            return FSPGM(mqtt_component_switch);
    }
}
