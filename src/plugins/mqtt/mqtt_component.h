/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include "mqtt_auto_discovery.h"

// use abbreviations to reduce the size of the auto discovery
#ifndef MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
#define MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS   1
#endif

PROGMEM_STRING_DECL(mqtt_component_switch);
PROGMEM_STRING_DECL(mqtt_component_light);
PROGMEM_STRING_DECL(mqtt_component_sensor);
PROGMEM_STRING_DECL(mqtt_component_binary_sensor);
PROGMEM_STRING_DECL(mqtt_component_storage);
PROGMEM_STRING_DECL(mqtt_unique_id);
PROGMEM_STRING_DECL(mqtt_name);
PROGMEM_STRING_DECL(mqtt_availability_topic);
PROGMEM_STRING_DECL(mqtt_topic);
PROGMEM_STRING_DECL(mqtt_status_topic);
PROGMEM_STRING_DECL(mqtt_payload_available);
PROGMEM_STRING_DECL(mqtt_payload_not_available);
PROGMEM_STRING_DECL(mqtt_state_topic);
PROGMEM_STRING_DECL(mqtt_command_topic);
PROGMEM_STRING_DECL(mqtt_payload_on);
PROGMEM_STRING_DECL(mqtt_payload_off);
PROGMEM_STRING_DECL(mqtt_brightness_state_topic);
PROGMEM_STRING_DECL(mqtt_brightness_command_topic);
PROGMEM_STRING_DECL(mqtt_brightness_scale);
PROGMEM_STRING_DECL(mqtt_color_temp_state_topic);
PROGMEM_STRING_DECL(mqtt_color_temp_command_topic);
PROGMEM_STRING_DECL(mqtt_rgb_state_topic);
PROGMEM_STRING_DECL(mqtt_rgb_command_topic);
PROGMEM_STRING_DECL(mqtt_unit_of_measurement);
PROGMEM_STRING_DECL(mqtt_value_template);

#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS

#define MQTT_DEVICE_REG_CONNECTIONS             "cns"
#define MQTT_DEVICE_REG_IDENTIFIERS             "ids"
#define MQTT_DEVICE_REG_NAME                    "name"
#define MQTT_DEVICE_REG_MANUFACTURER            "mf"
#define MQTT_DEVICE_REG_MODEL                   "mdl"
#define MQTT_DEVICE_REG_SW_VERSION              "sw"

#else

#define MQTT_DEVICE_REG_CONNECTIONS             "connections"
#define MQTT_DEVICE_REG_IDENTIFIERS             "identifiers"
#define MQTT_DEVICE_REG_NAME                    "name"
#define MQTT_DEVICE_REG_MANUFACTURER            "manufacturer"
#define MQTT_DEVICE_REG_MODEL                   "model"
#define MQTT_DEVICE_REG_SW_VERSION              "sw_version"

#endif

class MQTTClient;
class MQTTAutoDiscoveryQueue;

class MQTTComponent {
public:
    using MQTTAutoDiscoveryPtr = MQTTAutoDiscovery *;
    using ComponentTypeEnum_t = MQTTAutoDiscovery::ComponentTypeEnum_t;
    using ComponentType = MQTTAutoDiscovery::ComponentType;
    using Ptr = MQTTComponent *;
    using Vector = std::vector<Ptr>;
    using NameType = const __FlashStringHelper *;

    MQTTComponent(ComponentType type);
    virtual ~MQTTComponent();

#if MQTT_AUTO_DISCOVERY
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) = 0;
    virtual uint8_t getAutoDiscoveryCount() const = 0;

    uint8_t rewindAutoDiscovery();
    uint8_t getAutoDiscoveryNumber();
#endif

    virtual void onConnect(MQTTClient *client);
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

    NameType getName() const {
        return getNameByType(_type);
    }
    ComponentType getType() const {
        return _type;
    }

    static NameType getNameByType(ComponentType type);

private:
    ComponentType _type;
#if MQTT_AUTO_DISCOVERY
    friend MQTTAutoDiscoveryQueue;
    uint8_t _autoDiscoveryNum;
#endif
};
