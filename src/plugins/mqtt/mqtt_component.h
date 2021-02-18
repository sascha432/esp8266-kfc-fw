/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <EventScheduler.h>
#include "mqtt_strings.h"
#include "mqtt_auto_discovery.h"

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
    uint8_t getAutoDiscoveryNumber() const {
        return _autoDiscoveryNum;
    }
    void nextAutoDiscovery() {
        _autoDiscoveryNum++;
    }
    void retryAutoDiscovery() {
        _autoDiscoveryNum--;
    }
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
