/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "mqtt_def.h"
#include "kfc_fw_config.h"

namespace MQTT {

    class Component;
    class ComponentProxy;
    class PersistantStorageComponent;
    class AutoDiscoveryQueue;
    class AutoDiscovery;
    class Topic;
    class Queue;
    class Plugin;
    class Client;

    enum class ComponentType : uint8_t {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
        DEVICE_AUTOMATION
    };

    enum class FormatType : uint8_t {
        JSON,
        YAML,
        TOPIC,
    };

    enum class QueueType : uint8_t {
        SUBSCRIBE,
        UNSUBSCRIBE,
        PUBLISH
    };

    enum class StorageFrequencyType : uint8_t {
        DAILY,
        WEEKLY,
        MONTHLY
    };

    enum class ConnectionState : uint8_t {
        NONE = 0,
        IDLE,                           // not connecting and not about to connecft
        AUTO_RECONNECT_DELAY,           // waiting for reconnect
        PRE_CONNECT,                    // connect method called but otherwise unknown state
        CONNECTING,                     // disconnected and attempting to connect
        DISCONNECTING,                  // disconnecting, waiting for tcp connection to close
        CONNECTED,                      // connection established and aknownledges by the mqtt server
        DISCONNECTED                    // disconnect callback received
    };

    using AutoDiscoveryPtr = AutoDiscovery *;
    using AutoReconnectType = uint16_t;
    using AutoDiscoveryQueuePtr = std::unique_ptr<AutoDiscoveryQueue>;
    using ComponentPtr = Component *;
    using ComponentVector = std::vector<ComponentPtr>;
    using ConfigType = KFCConfigurationClasses::Plugins::MQTTClient::MqttConfig_t;
    using ModeType = KFCConfigurationClasses::Plugins::MQTTClient::ModeType;
    using QosType = KFCConfigurationClasses::Plugins::MQTTClient::QosType;
    using ClientConfig = KFCConfigurationClasses::Plugins::MQTTClient;
    using ResultCallback = std::function<void(ComponentPtr component, Client *client, bool result)>;
    using ComponentProxyPtr = std::unique_ptr<ComponentProxy>;
    using NameType = const __FlashStringHelper *;

    static constexpr AutoReconnectType kAutoReconnectDisabled = 0;

}

using MQTTPlugin = MQTT::Plugin;
using MQTTClient = MQTT::Client;
