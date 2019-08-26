/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#include "mqtt_component.h"

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
PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_measurement");
PROGMEM_STRING_DEF(mqtt_value_template, "value_template");

MQTTComponent::MQTTComponent(ComponentTypeEnum_t type) : _type(type), _num(0xff) {
}

MQTTComponent::~MQTTComponent() {
}

void MQTTComponent::onConnect(MQTTClient *client) {
}

void MQTTComponent::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) {
}

void MQTTComponent::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
}

PGM_P MQTTComponent::getComponentName() {
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


MQTTComponentHelper::MQTTComponentHelper(ComponentTypeEnum_t type) : MQTTComponent(type) {
}

void MQTTComponentHelper::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
}

MQTTAutoDiscovery *MQTTComponentHelper::createAutoDiscovery(MQTTAutoDiscovery::Format_t format) {
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    return discovery;
}

#endif
