/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include "mqtt_auto_discovery.h"

PROGMEM_STRING_DECL(mqtt_component_switch);
PROGMEM_STRING_DECL(mqtt_component_light);
PROGMEM_STRING_DECL(mqtt_component_sensor);
PROGMEM_STRING_DECL(mqtt_component_binary_sensor);

PROGMEM_STRING_DECL(mqtt_unique_id);
PROGMEM_STRING_DECL(mqtt_availability_topic);
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

class MQTTClient;

class MQTTComponent {
public:
    virtual ~MQTTComponent() {
    }

    virtual const String getName() = 0;
    virtual PGM_P getComponentName() = 0;
    virtual MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format = MQTTAutoDiscovery::FORMAT_JSON) = 0;

    virtual void onConnect(MQTTClient *client) {
    }
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) {
    }
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
    }

    void setNumber(uint8_t num) {
        _num = num;
    }
    uint8_t getNumber() {
        return _num;
    }

private:
    uint8_t _num;
};

#endif
