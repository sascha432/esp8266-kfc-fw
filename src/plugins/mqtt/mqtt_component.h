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
PROGMEM_STRING_DECL(mqtt_name);
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
PROGMEM_STRING_DECL(mqtt_value_template);

class MQTTClient;

class MQTTComponent {
public:
    typedef std::unique_ptr<MQTTAutoDiscovery> MQTTAutoDiscoveryPtr;
    typedef std::vector<MQTTAutoDiscoveryPtr> MQTTAutoDiscoveryVector;

    typedef enum {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
    } ComponentTypeEnum_t;

    MQTTComponent(ComponentTypeEnum_t type);
    virtual ~MQTTComponent();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) = 0;
    virtual uint8_t getAutoDiscoveryCount() const = 0;

    virtual void onConnect(MQTTClient *client);
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

#if MQTT_AUTO_DISCOVERY
    void publishAutoDiscovery(MQTTClient *client);
#endif

    PGM_P getComponentName();

    inline void setNumber(uint8_t num) {
        _num = num;
    }
    inline uint8_t getNumber() {
        return _num;
    }

private:
    ComponentTypeEnum_t _type;
    uint8_t _num;
};

// for creating auto discovery without an actual component
class MQTTComponentHelper : public MQTTComponent {
public:
    MQTTComponentHelper(ComponentTypeEnum_t type);
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    MQTTAutoDiscovery *createAutoDiscovery(uint8_t count, MQTTAutoDiscovery::Format_t format);
};

#endif
