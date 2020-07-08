/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <EventScheduler.h>
#include <EventTimer.h>
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
PROGMEM_STRING_DECL(mqtt_rgb_state_topic);
PROGMEM_STRING_DECL(mqtt_rgb_command_topic);
PROGMEM_STRING_DECL(mqtt_unit_of_measurement);
PROGMEM_STRING_DECL(mqtt_value_template);

class MQTTClient;

class MQTTComponent {
public:
    using MQTTAutoDiscoveryPtr = MQTTAutoDiscovery *;
    using ComponentTypeEnum_t = MQTTAutoDiscovery::ComponentTypeEnum_t;
    using Ptr = MQTTComponent *;
    using Vector = std::vector<Ptr>;

    MQTTComponent(ComponentTypeEnum_t type);
    virtual ~MQTTComponent();

#if MQTT_AUTO_DISCOVERY
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) = 0;
    virtual uint8_t getAutoDiscoveryCount() const = 0;

    uint8_t rewindAutoDiscovery();
    uint8_t getAutoDiscoveryNumber();
#endif

    virtual void onConnect(MQTTClient *client);
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

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
#if MQTT_AUTO_DISCOVERY
    friend class MQTTAutoDiscoveryQueue;
    uint8_t _autoDiscoveryNum;
#endif
};

// for creating auto discovery without an actual component
class MQTTComponentHelper : public MQTTComponent {
public:
    MQTTComponentHelper(ComponentTypeEnum_t type);
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    MQTTAutoDiscovery *createAutoDiscovery(uint8_t count, MQTTAutoDiscovery::Format_t format);
    MQTTAutoDiscovery *createAutoDiscovery(const String &componentName, MQTTAutoDiscovery::Format_t format);
};
