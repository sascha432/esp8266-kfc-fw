/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MQTT_SUPPORT

#include <Arduino_compat.h>
#include <LString.h>
#include <vector>
#include <memory>
#include "mqtt_component.h"

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

    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format = MQTTAutoDiscovery::FORMAT_JSON) override {
        return nullptr;
    }

    void onConnect(MQTTClient *client);
    void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

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
