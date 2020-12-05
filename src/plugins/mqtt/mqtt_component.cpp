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

// #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS

// PROGMEM_STRING_DEF(mqtt_unique_id, "uniq_id");
// PROGMEM_STRING_DEF(mqtt_availability_topic, "avty_t");
// PROGMEM_STRING_DEF(mqtt_payload_available, "pl_avail");
// PROGMEM_STRING_DEF(mqtt_payload_not_available, "pl_not_avail");
// PROGMEM_STRING_DEF(mqtt_topic, "t");
// PROGMEM_STRING_DEF(mqtt_state_topic, "stat_t");
// PROGMEM_STRING_DEF(mqtt_command_topic, "cmd_t");
// PROGMEM_STRING_DEF(mqtt_payload_on, "pl_on");
// PROGMEM_STRING_DEF(mqtt_payload_off, "pl_off");
// PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "bri_stat_t");
// PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "bri_cmd_t");
// PROGMEM_STRING_DEF(mqtt_brightness_scale, "bri_scl");
// PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "clr_temp_stat_t");
// PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "clr_temp_cmd_t");
// PROGMEM_STRING_DEF(mqtt_rgb_state_topic, "rgb_stat_t");
// PROGMEM_STRING_DEF(mqtt_rgb_command_topic, "rgb_cmd_t");
// PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_meas");
// PROGMEM_STRING_DEF(mqtt_value_template, "val_tpl");
// PROGMEM_STRING_DEF(mqtt_expire_after, "exp_aft");
// PROGMEM_STRING_DEF(mqtt_icon, "ic");
// PROGMEM_STRING_DEF(mqtt_force_update, "frc_upd");
// PROGMEM_STRING_DEF(mqtt_json_attributes_template, "");
// PROGMEM_STRING_DEF(mqtt_json_attributes_topic, "");

// #else

// PROGMEM_STRING_DEF(mqtt_unique_id, "unique_id");
// PROGMEM_STRING_DEF(mqtt_availability_topic, "availability_topic");
// PROGMEM_STRING_DEF(mqtt_payload_available, "payload_available");
// PROGMEM_STRING_DEF(mqtt_payload_not_available, "payload_not_available");
// PROGMEM_STRING_DEF(mqtt_topic, "topic");
// PROGMEM_STRING_DEF(mqtt_state_topic, "state_topic");
// PROGMEM_STRING_DEF(mqtt_command_topic, "command_topic");
// PROGMEM_STRING_DEF(mqtt_payload_on, "payload_on");
// PROGMEM_STRING_DEF(mqtt_payload_off, "payload_off");
// PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "brightness_state_topic");
// PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "brightness_command_topic");
// PROGMEM_STRING_DEF(mqtt_brightness_scale, "brightness_scale");
// PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "color_temp_state_topic");
// PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "color_temp_command_topic");
// PROGMEM_STRING_DEF(mqtt_rgb_state_topic, "rgb_state_topic");
// PROGMEM_STRING_DEF(mqtt_rgb_command_topic, "rgb_command_topic");
// PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_measurement");
// PROGMEM_STRING_DEF(mqtt_value_template, "value_template");
// PROGMEM_STRING_DEF(mqtt_expire_after, "expire_after");
// PROGMEM_STRING_DEF(mqtt_icon, "icon");
// PROGMEM_STRING_DEF(mqtt_force_update, "force_update");
// PROGMEM_STRING_DEF(mqtt_json_attributes_template, "json_attributes_template");
// PROGMEM_STRING_DEF(mqtt_json_attributes_topic, "json_attributes_topic");

// #endif

MQTTComponent::MQTTComponent(ComponentTypeEnum_t type) : _type(type), _autoDiscoveryNum(0)
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

uint8_t MQTTComponent::getAutoDiscoveryNumber()
{
    return _autoDiscoveryNum++;
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
        case ComponentType::STORAGE:
            return FSPGM(mqtt_component_storage);
        case ComponentType::SWITCH:
        default:
            return FSPGM(mqtt_component_switch);
    }
}
