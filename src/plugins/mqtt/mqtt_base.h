/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "mqtt_def.h"
#include "kfc_fw_config.h"

namespace MQTT {

    enum class ComponentType : uint8_t {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
        DEVICE_AUTOMATION,
        FAN,
        AUTO_DISCOVERY
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

    enum class PacketAckType : uint8_t {
        SUBSCRIBE,
        UNSUBCRIBE,
        PUBLISH,
        TIMEOUT,
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

    // enum class AutoDiscoveryFlags : uint8_t {
    //     FORCE_RUN    = 0x01,
    //     ABORT_RUN    = 0x02,
    //     FORCE_UPDATE = 0x04,
    //     REMOVE_ALL   = 0x08,
    //     UPDATE_ALL   = 0x10,
    //     _DEFAULT     = UPDATE_ALL,
    // };

    enum class StatusType {
        STARTED,
        DEFERRED,
        SUCCESS,
        FAILURE,
    };

    enum class RunFlags : uint8_t {
        NONE = 0,
        FORCE_NOW = 0x01,                    // ignore any delays and start auto discovery
        FORCE_UPDATE = 0x02,                 // resend entire auto discovery without checking for changes
        ABORT_RUNNING = 0x04,                // stop the autodiscovery that is currently running
        FORCE = FORCE_NOW|FORCE_UPDATE|ABORT_RUNNING,
        DEFAULTS = NONE,
    };

    using RunFlagsUnderlyingType = typename std::underlying_type<RunFlags>::type;

    inline static RunFlagsUnderlyingType operator&(const RunFlags & left, const RunFlags &right)
    {
        return static_cast<RunFlagsUnderlyingType>(left) & static_cast<RunFlagsUnderlyingType>(right);
    }

    inline static RunFlags &operator|=(RunFlags &flags, RunFlags value)
    {
        return (flags = static_cast<RunFlags>(static_cast<RunFlagsUnderlyingType>(flags) | static_cast<RunFlagsUnderlyingType>(value)));
    }

    class Component;
    class ComponentProxy;
    class ComponentIterator;
    class PersistantStorageComponent;
    class Plugin;
    class Client;
    class ClientTopic;
    class ClientQueue;
    class PacketQueue;
    class QueueVector;
    class PacketQueueVector;

    namespace AutoDiscovery {

        class Entity;
        class Queue;
        class List;
        class ListIterator;
        class ConstListIterator;

        using StatusCallback = std::function<void(StatusType status)>;

        using EntityPtr = Entity *;
        using EntitySharedPtr = std::shared_ptr<Entity>;
        using QueuePtr = std::unique_ptr<Queue>;
    };


    using AutoReconnectType = uint16_t;
    using ComponentPtr = Component *;
    using ClientPtr = Client *;
    using NameType = const __FlashStringHelper *;
    using ComponentVector = std::vector<ComponentPtr>;
    using ConfigType = KFCConfigurationClasses::Plugins::MQTTClient::MqttConfig_t;
    using ModeType = KFCConfigurationClasses::Plugins::MQTTClient::ModeType;
    using QosType = KFCConfigurationClasses::Plugins::MQTTClient::QosType;
    using ClientConfig = KFCConfigurationClasses::Plugins::MQTTClient;
    using ResultCallback = std::function<void(ComponentPtr component, Client *client, bool result)>;
    using ComponentProxyPtr = std::unique_ptr<ComponentProxy>;
    using TopicVector = std::vector<ClientTopic>;
    // using QueueVector = std::vector<ClientQueue>;
    // using PacketQueueList = std::vector<PacketQueue>;

    static constexpr AutoReconnectType kAutoReconnectDisabled = 0;

    // reduce timeouts if the queue reqiures too much RAM

    // packets in the local queue are discarded after 7.5 seconds
    // the timeout can be changed after sending a packet
    // _packetQueue.setTimeout(internalid, timeout);
    static constexpr uint16_t kDefaultQueueTimeout = MQTT_QUEUE_TIMEOUT;            // milliseconds
    // if a packet does not get acknowledged within 10 seconds, it times out
    static constexpr uint16_t kDefaultAckTimeout = MQTT_DEFAULT_TIMEOUT;            // milliseconds
    // if the packet is in the delivery queue, the timeout increase by kDefaultAckTimeout
    static constexpr uint16_t kDefaultCanDeleteTimeout = kDefaultAckTimeout;        // milliseconds


    // delay after calling publish() without force = true
    static constexpr uint32_t kAutoDiscoveryInitialDelay = MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY; // milliseconds

    static constexpr uint32_t kAutoDiscoveryErrorDelay = MQTT_AUTO_DISCOVERY_ERROR_DELAY; // milliseconds


    static constexpr uint32_t kDeliveryQueueRetryDelay = MQTT_QUEUE_RETRY_DELAY; // milliseconds


}

using MQTTPlugin = MQTT::Plugin;
using MQTTClient = MQTT::Client;
