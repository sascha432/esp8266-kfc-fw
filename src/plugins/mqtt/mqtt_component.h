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
PROGMEM_STRING_DECL(mqtt_unit_of_measurement);

class MQTTClient;

class MQTTComponent {
public:
    typedef enum {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
    } ComponentTypeEnum_t;

    MQTTComponent(ComponentTypeEnum_t type) : _type(type), _num(0xff) {
    }
    virtual ~MQTTComponent() {
    }

    virtual MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format = MQTTAutoDiscovery::FORMAT_JSON) = 0;

    virtual void onConnect(MQTTClient *client) {
    }
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) {
    }
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
    }

    inline void setNumber(uint8_t num) {
        _num = num;
    }
    inline uint8_t getNumber() {
        return _num;
    }

    PGM_P getComponentName() {
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

private:
    ComponentTypeEnum_t _type;
    uint8_t _num;
};


// for creating auto discovery without an actual component
class MQTTComponentHelper : public MQTTComponent {
public:
    MQTTComponentHelper(ComponentTypeEnum_t type) : MQTTComponent(type) {

    }

    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format = MQTTAutoDiscovery::FORMAT_JSON) override {
        auto discovery = _debug_new MQTTAutoDiscovery();
        discovery->create(this, format);
        return discovery;
    }
};

#endif
