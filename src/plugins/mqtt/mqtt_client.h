/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MQTT_CLIENT
#define DEBUG_MQTT_CLIENT                       0
#endif

// home assistant auto discovery
#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY                     1
#endif

// support for MQTT group topic
#ifndef MQTT_GROUP_TOPIC
#define MQTT_GROUP_TOPIC                        0
#endif

// 0 to disable last will. status is set to offline when disconnect(false) is called
// useful for devices that go to deep sleep and stay "online"
#ifndef MQTT_SET_LAST_WILL
#define MQTT_SET_LAST_WILL                      1
#endif

// enable callbacks for QoS. onSubscribe(), onPublish() and onUnsubscribe()
#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS               0
#endif

// timeout if subscribe/unsubcribe/publish cannot be sent
#ifndef MQTT_QUEUE_TIMEOUT
#define MQTT_QUEUE_TIMEOUT                      7500
#endif

// delay before retrying subscribe/unsubcribe/publish
#ifndef MQTT_QUEUE_RETRY_DELAY
#define MQTT_QUEUE_RETRY_DELAY                  250
#endif

// maximum size of topc + payload + header (~7 byte) + safety limit for sending messages
#ifndef MQTT_MAX_MESSAGE_SIZE
#define MQTT_MAX_MESSAGE_SIZE                   (TCP_SND_BUF - 64)
#endif

// incoming message that exceed the limit are discarded
#ifndef MQTT_RECV_MAX_MESSAGE_SIZE
#define MQTT_RECV_MAX_MESSAGE_SIZE              1024
#endif

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <FixedString.h>
#include <vector>
#include "EnumBase.h"
#include "kfc_fw_config.h"
#include "mqtt_component.h"
#include "mqtt_auto_discovery.h"
#include "mqtt_auto_discovery_queue.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


class MQTTPersistantStorageComponent;
class MQTTPlugin;
class MQTTClient {
public:
    class MQTTTopic;
    class MQTTQueue;

    using ComponentPtr = MQTTComponent::Ptr;
    using ComponentVector = MQTTComponent::Vector;
    using ConfigType = KFCConfigurationClasses::Plugins::MQTTClient::MqttConfig_t;
    using ModeType = KFCConfigurationClasses::Plugins::MQTTClient::ModeType;
    using QosType = KFCConfigurationClasses::Plugins::MQTTClient::QosType;
    using ClientConfig = KFCConfigurationClasses::Plugins::MQTTClient;

    using AutoReconnectType = uint16_t;
    using TopicVector = std::vector<MQTTTopic>;
    using QueueVector = std::vector<MQTTQueue>; // this is not used for QoS at the moment

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

    static constexpr AutoReconnectType kAutoReconnectDisabled = 0;

    class MQTTTopic {
    public:
        MQTTTopic(const String &topic, MQTTComponent *component) : _topic(topic), _component(component) {
        }
        inline const String &getTopic() const {
            return _topic;
        }
        inline MQTTComponent *getComponent() const {
            return _component;
        }
        inline bool match(MQTTComponent *component, const String &topic) const {
            return (component == _component) && (topic == _topic);
        }
    private:
        String _topic;
        MQTTComponent *_component;
    };

    class MQTTQueue {
    public:
        MQTTQueue(QueueType type, MQTTComponent *component, const String &topic, QosType qos, bool retain, const String &payload) :
            _component(component),
            _topic(topic),
            _payload(payload),
            _type(type),
            _qos(qos),
            _retain(retain)
        {
        }

        inline QueueType getType() const {
            return _type;
        }
        inline MQTTComponent *getComponent() const {
            return _component;
        }
        inline const String &getTopic() const {
            return _topic;
        }
        inline QosType getQos() const {
            return _qos;
        }
        inline bool getRetain() const {
            return _retain;
        }
        inline const String &getPayload() const {
            return _payload;
        }
    private:
        MQTTComponent *_component;
        String _topic;
        String _payload;
        QueueType _type;
        QosType _qos;
        bool _retain: 1;
    };

    MQTTClient();
    virtual ~MQTTClient();

    static void setupInstance();
    static void deleteInstance();

    void registerComponent(ComponentPtr component);
    bool unregisterComponent(ComponentPtr component);
    bool isComponentRegistered(ComponentPtr component);

    // connect to server if not connected/connecting
    void connect();
    // disconnect from server
    void disconnect(bool forceDisconnect = false);
    // set last will to mark component as offline on disconnect
    // sets payload and topic
    template<typename _T>
    void setLastWillPayload(_T str) {
        // last will topic and payload pointer must be valid until the connection has been closed
        _lastWillTopic = formatTopic(FSPGM(mqtt_status_topic));
        _lastWillPayload = str;
    }
    // publish last will
    // setLastWillPayload must be called before
    void publishLastWill();
    // returns true for ConnectionState::CONNECTED,
    bool isConnected() const;
    // returns true for ConnectionState::PRE_CONNECT, CONNECTING and CONNECTED
    bool isConnecting() const;
    // set time to wait until next reconnect attempt. 0 = disable auto reconnect
    void setAutoReconnect(AutoReconnectType timeout);
    // timeout = minimum value or 0
    void setAutoReconnect();

    // <mqtt_topic=home/${device_name}>/component_name><format>
    // NOTE: there is no slash between the component name and format
    static String formatTopic(const String &componentName, const __FlashStringHelper *format = nullptr, ...);

    static String formatTopic(const __FlashStringHelper *format = nullptr, ...);

public:
    void subscribe(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
    void unsubscribe(ComponentPtr component, const String &topic);
#if MQTT_GROUP_TOPIC
    void subscribeWithGroup(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
    void unsubscribeWithGroup(ComponentPtr component, const String &topic);
#endif
    void remove(ComponentPtr component);
    void publish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);
    void publishPersistantStorage(StorageFrequencyType type, const String &name, const String &data);

public:
    // force starts to send the auto discovery ignoring the delay between each run
    // returns false if running
    bool publishAutoDiscovery(bool force = false);
    static void publishAutoDiscoveryCallback(Event::CallbackTimerPtr timer);

public:
    // return values
    // 0: failed to send
    // -1: success, but nothing was sent (in particular for unsubscribe if another component is still subscribed to the same topic)
    // for qos 1 and 2: >= 1 packet id, confirmation will be received by callbacks once delivered
    // for qos 0: 1 = successfully delivered to tcp stack
    int subscribeWithId(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
    int unsubscribeWithId(ComponentPtr component, const String &topic);
    int publishWithId(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);

    static void handleWiFiEvents(WiFiCallbacks::EventType event, void *payload);

public:
    // methods that can be called without verifying getClient() is not nullptr
    static MQTTClient *getClient();
    static void safePublish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);
    static void safeRegisterComponent(ComponentPtr component);
    static bool safeUnregisterComponent(ComponentPtr component);
    static void safeReRegisterComponent(ComponentPtr component);
    static void safePersistantStorage(StorageFrequencyType type, const String &name, const String &data);

public:
    // returns 1 for:
    // any integer != 0
    // "true"
    // "on"
    // "yes"
    // "online"
    // "enable"
    // "enabled"

    // returns 0 for:
    // 0
    // "00"...
    // "false"
    // "off"
    // "no"
    // "offline"
    // "disable"
    // "disabled"

    // otherwise it returns -1 or "invalid"
    // white spaces are stripped and the strings are case insensitive
    static int8_t toBool(const char *, int8_t invalid = -1);

    static QosType getDefaultQos(QosType qos = QosType::DEFAULT);

    String connectionDetailsString();
    String connectionStatusString();

    void _handleWiFiEvents(WiFiCallbacks::EventType event, void *payload);
    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len);

#if MQTT_USE_PACKET_CALLBACKS
    void onPublish(uint16_t packetId);
    void onSubscribe(uint16_t packetId, uint8_t qos);
    void onUnsubscribe(uint16_t packetId);
#endif

    TopicVector &getTopics();
    size_t getQueueSize() const;

public:
    // return qos if not QosType::DEFAULT, otherwise return the qos value from the configuration
    static QosType _getDefaultQos(QosType qos = QosType::DEFAULT);
    // translate into 0, 1 or 2
    static uint8_t _translateQosType(QosType qos = QosType::DEFAULT);

private:
    static String _getBaseTopic();
#if MQTT_GROUP_TOPIC
    static bool _getGroupTopic(ComponentPtr component, String groupTopic, String &topic);
#endif
    static String _formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg);
    static String _filterString(const char *str, bool replaceSpace = false);

private:
    void _zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type);
    void _setupClient();
    void autoReconnect(AutoReconnectType timeout);

    const __FlashStringHelper *_reasonToString(AsyncMqttClientDisconnectReason reason) const;

    // match wild cards
    bool _isTopicMatch(const char *topic, const char *match) const;
    // check if the topic is in use by another component
    bool _topicInUse(ComponentPtr component, const String &topic);

private:
    // if subscribe/unsubscribe/publish fails cause of the tcp client's buffer being full, retry later
    // the payload can be quite large for auto discovery for example
    // messages might be delivered in a different sequence. use subscribe/unsubscribe/publishWithId for full control
    // to deliver messages in sequence, use QoS and callbacks (onPublish() etc... requires MQTT_USE_PACKET_CALLBACKS=1)
    void _addQueue(QueueType type, MQTTComponent *component, const String &topic, QosType qos, bool retain, const String &payload);
    // stop timer and clear queue
    void _clearQueue();
    // process queue
    void _queueTimerCallback();

    QueueVector _queue;
    Event::Timer _queueTimer;

private:
    friend MQTTAutoDiscoveryQueue;
    friend MQTTPersistantStorageComponent;
    friend MQTTPlugin;

    size_t getClientSpace() const;
    static bool _isMessageSizeExceeded(size_t len, const char *topic);
    ConnectionState setConnState(ConnectionState newState);

    String _hostname;
    IPAddress _address;
    String _username;
    String _password;
    ConfigType _config;
    AsyncMqttClient *_client;
    Event::Timer _timer;
    uint16_t _port;
    uint16_t _componentsEntityCount;
    FixedString<1> _lastWillPayload;
    AutoReconnectType _autoReconnectTimeout;
    ComponentVector _components;
    TopicVector _topics;
    Buffer _buffer;
    String _lastWillTopic;
    std::unique_ptr<MQTTAutoDiscoveryQueue> _autoDiscoveryQueue;
    Event::Timer _autoDiscoveryRebroadcast;
    ConnectionState _connState;

#if DEBUG_MQTT_CLIENT
public:
    const __FlashStringHelper *_getConnStateStr();
    const char *_connection();
    String _connectionStr;
#endif

private:
    static MQTTClient *_mqttClient;
};


inline void MQTTClient::setAutoReconnect(AutoReconnectType timeout)
{
    __LDBG_printf("timeout=%d conn=%s", timeout, _connection());
    _autoReconnectTimeout = timeout;
}

inline void MQTTClient::setAutoReconnect()
{
    setAutoReconnect(_config.auto_reconnect_min);
}

inline bool MQTTClient::isConnected() const
{
    return _connState == ConnectionState::CONNECTED;
}

inline size_t MQTTClient::getClientSpace() const
{
    return _client->getAsyncClient().space();
}

inline MQTTClient::ConnectionState MQTTClient::setConnState(ConnectionState newState)
{
    auto tmp = _connState;
    _connState = newState;
    return tmp;
}

inline MQTTClient::TopicVector &MQTTClient::getTopics()
{
    return _topics;
}

inline size_t MQTTClient::getQueueSize() const
{
    return _queue.size();
}

inline void MQTTClient::publishAutoDiscoveryCallback(Event::CallbackTimerPtr timer)
{
    if (_mqttClient) {
        _mqttClient->publishAutoDiscovery();
    }
}

inline void MQTTClient::handleWiFiEvents(WiFiCallbacks::EventType event, void *payload)
{
    if (_mqttClient) {
        _mqttClient->_handleWiFiEvents(event, payload);
    }
}

inline MQTTClient *MQTTClient::getClient()
{
    return _mqttClient;
}

inline void MQTTClient::safePublish(const String &topic, bool retain, const String &payload, QosType qos)
{
    if (_mqttClient && _mqttClient->isConnected()) {
        _mqttClient->publish(topic, retain, payload, qos);
    }
}

inline void MQTTClient::safeRegisterComponent(ComponentPtr component)
{
    if (_mqttClient) {
        _mqttClient->registerComponent(component);
    }
}

inline bool MQTTClient::safeUnregisterComponent(ComponentPtr component)
{
    if (_mqttClient) {
        return _mqttClient->unregisterComponent(component);
    }
    return false;
}

inline void MQTTClient::safeReRegisterComponent(ComponentPtr component)
{
    if (_mqttClient) {
        _mqttClient->unregisterComponent(component);
        _mqttClient->registerComponent(component);
    }
}

inline void MQTTClient::safePersistantStorage(StorageFrequencyType type, const String &name, const String &data)
{
    if (_mqttClient) {
        _mqttClient->publishPersistantStorage(type, name, data);
    }
}

inline MQTTClient::QosType MQTTClient::getDefaultQos(QosType qos)
{
    return qos;
}

inline MQTTClient::QosType MQTTClient::_getDefaultQos(QosType qos)
{
    if (qos != QosType::DEFAULT) {
        return qos;
    }
    if (_mqttClient) {
        return static_cast<QosType>(_mqttClient->_config.qos);
    }
    return static_cast<QosType>(ClientConfig::getConfig().qos);
}

inline uint8_t MQTTClient::_translateQosType(QosType qos)
{
    return static_cast<uint8_t>(_getDefaultQos(qos));
}

#include <debug_helper_disable.h>
