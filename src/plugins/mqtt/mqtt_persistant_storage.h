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
    using Container = KeyValueStorage::Container;
    using ContainerPtr = KeyValueStorage::ContainerPtr;

private:
    MQTTPersistantStorageComponent(ContainerPtr data, Callback callback);
public:
    ~MQTTPersistantStorageComponent();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) {
        return nullptr;
    }
    virtual uint8_t getAutoDiscoveryCount() const {
        return 0;
    }

    virtual void onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

public:
    static bool create(MQTTClient *client, ContainerPtr data, Callback callback);
    static void remove(MQTTPersistantStorageComponent *component);
    static bool isActive();

private:
    bool _begin(MQTTClient *client);
    void _end(MQTTClient *client);
    void _remove();

    String _topic;
    ContainerPtr _data;
    Callback _callback;
    EventScheduler::Timer _timer;
    bool _ignoreMessages;

    static bool _active;
};
