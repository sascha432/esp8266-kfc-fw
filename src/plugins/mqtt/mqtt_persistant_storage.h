/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "mqtt_component.h"
#include "kfc_fw_config.h"

#ifndef DEBUG_MQTT_PERSISTANT_STORAGE
#define DEBUG_MQTT_PERSISTANT_STORAGE                       1
#endif

// timeout after successfully subscribing to the persistant storage topic
#ifndef MQTT_PERSISTANT_STORAGE_TIMEOUT
#define MQTT_PERSISTANT_STORAGE_TIMEOUT                     5000
#endif

class MQTTPersistantStorageComponent : public MQTTComponent
{
public:
    using Callback = KFCFWConfiguration::PersistantConfigCallback;
    using Vector = KFCFWConfiguration::KeyValueStoreVector;
    using VectorPtr = KFCFWConfiguration::KeyValueStoreVectorPtr;

private:
    MQTTPersistantStorageComponent(VectorPtr data, Callback callback);
public:
    ~MQTTPersistantStorageComponent();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) {
        return nullptr;
    }
    virtual uint8_t getAutoDiscoveryCount() const {
        return 0;
    }

    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

public:
    static bool create(MQTTClient *client, VectorPtr data, Callback callback);
    static bool isActive();

private:
    bool _begin(MQTTClient *client);
    String _serialize(VectorPtr data);
    VectorPtr _unserialize(const char *str);
    void _merge(VectorPtr target, VectorPtr data);

    String _topic;
    VectorPtr _data;
    Callback _callback;
    EventScheduler::Timer _timer;
    bool _ignoreMessages;

    static bool _active;
};
