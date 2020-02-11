/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#pragma once

// Auto Discovery Client for MQTT
// The client collects auto discovery information for the topic "mqtt.discovery_prefix"

#include <Arduino_compat.h>
#include <LString.h>
#include <vector>
#include <memory>
#include "mqtt_component.h"

#if MQTT_AUTO_DISCOVERY_CLIENT

class MQTTAutoDiscoveryClient : public MQTTComponent {
public:
    typedef struct {
        uint16_t id;
        String topic;
        String payload;
        String name;
        LString swVersion;
        LString model;
        LString manufacturer;
    } Discovery_t;

    typedef std::unique_ptr<Discovery_t> DiscoveryPtr_t;
    typedef std::vector<DiscoveryPtr_t> DiscoveryVector;

private:
    MQTTAutoDiscoveryClient(Stream *stream = nullptr);

public:
    static MQTTAutoDiscoveryClient *createInstance(Stream *stream = nullptr);
    static void deleteInstance();

    static inline MQTTAutoDiscoveryClient *getInstance() {
        return _instance;
    }

    virtual ~MQTTAutoDiscoveryClient();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector);
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void onConnect(MQTTClient *client) override;
    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    inline DiscoveryVector &getDiscovery() {
        return _discovery;
    }

private:
    DiscoveryVector _discovery;
    Stream *_stream;

    static uint16_t _uniqueId;
    static MQTTAutoDiscoveryClient *_instance;
};

#endif

#endif
