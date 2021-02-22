/**
 * Author: sascha_lammers@gmx.de
 */

#include "component.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace MQTT;

Component::Component(ComponentType type) : _type(type), _autoDiscoveryNum(0)
{
}

Component::~Component()
{
}

void Component::onConnect(Client *client)
{
}

void Component::onDisconnect(Client *client, AsyncMqttClientDisconnectReason reason)
{
}

void Component::onMessage(Client *client, char *topic, char *payload, size_t len)
{
}

#if MQTT_AUTO_DISCOVERY

uint8_t Component::rewindAutoDiscovery()
{
    uint8_t count;
    if ((count = getAutoDiscoveryCount()) != 0) {
        _autoDiscoveryNum = 0;
        return true;
    }
    return false;
}

#endif

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
        case ComponentType::SWITCH:
        default:
            return FSPGM(mqtt_component_switch);
    }
}
