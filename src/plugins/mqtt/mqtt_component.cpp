/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#include "mqtt_component.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(mqtt_component_switch, "switch");
PROGMEM_STRING_DEF(mqtt_component_light, "light");
PROGMEM_STRING_DEF(mqtt_component_sensor, "sensor");
PROGMEM_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor");

PROGMEM_STRING_DEF(mqtt_unique_id, "unique_id");
PROGMEM_STRING_DEF(mqtt_name, "name");
PROGMEM_STRING_DEF(mqtt_availability_topic, "availability_topic");
PROGMEM_STRING_DEF(mqtt_status_topic, "/status");
PROGMEM_STRING_DEF(mqtt_payload_available, "payload_available");
PROGMEM_STRING_DEF(mqtt_payload_not_available, "payload_not_available");
PROGMEM_STRING_DEF(mqtt_state_topic, "state_topic");
PROGMEM_STRING_DEF(mqtt_command_topic, "command_topic");
PROGMEM_STRING_DEF(mqtt_payload_on, "payload_on");
PROGMEM_STRING_DEF(mqtt_payload_off, "payload_off");
PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "brightness_state_topic");
PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "brightness_command_topic");
PROGMEM_STRING_DEF(mqtt_brightness_scale, "brightness_scale");
PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "color_temp_state_topic");
PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "color_temp_command_topic");
PROGMEM_STRING_DEF(mqtt_rgb_state_topic, "rgb_state_topic");
PROGMEM_STRING_DEF(mqtt_rgb_command_topic, "rgb_command_topic");
PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_measurement");
PROGMEM_STRING_DEF(mqtt_value_template, "value_template");

MQTTComponent::MQTTComponent(ComponentTypeEnum_t type) : _type(type), _num(0xff)
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
void MQTTComponent::publishAutoDiscovery(MQTTClient *client)
{
    if (MQTTAutoDiscovery::isEnabled()) {
        MQTTAutoDiscoveryVector vector;
        createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON, vector);
        for(const auto &discovery: vector) {
            _debug_printf_P(PSTR("MQTTComponent::publishAutoDiscovery(): topic=%s, payload=%s\n"), discovery->getTopic().c_str(), discovery->getPayload().c_str());
            client->publish(discovery->getTopic(), client->getDefaultQos(), true, discovery->getPayload());
        }
    }

}
#endif

PGM_P MQTTComponent::getComponentName()
{
    switch(_type) {
        case LIGHT:
            return SPGM(mqtt_component_light);
        case SENSOR:
            return SPGM(mqtt_component_sensor);
        case BINARY_SENSOR:
            return SPGM(mqtt_component_binary_sensor);
        case SWITCH:
            break;

    }
    return SPGM(mqtt_component_switch);
}


MQTTComponentHelper::MQTTComponentHelper(ComponentTypeEnum_t type) : MQTTComponent(type)
{
}

void MQTTComponentHelper::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
}

uint8_t MQTTComponentHelper::getAutoDiscoveryCount() const
{
    return 0;
}

MQTTAutoDiscovery *MQTTComponentHelper::createAutoDiscovery(uint8_t count, MQTTAutoDiscovery::Format_t format)
{
    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, count, format);
    return discovery;
}

#endif
