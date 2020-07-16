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

// disable last will. status is set to offline when disconnecting only
#ifndef MQTT_SET_LAST_WILL
#define MQTT_SET_LAST_WILL                      1
#endif

// enable callbacks for QoS. onSubscribe(), onPublish() and onUnsubscribe()
#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS               0
#endif

#ifndef MQTT_AUTO_RECONNECT_TIMEOUT
#define MQTT_AUTO_RECONNECT_TIMEOUT             5000
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
#include <vector>
#include "EnumBase.h"
#include "kfc_fw_config.h"
#include "mqtt_component.h"
#include "mqtt_auto_discovery.h"
#include "mqtt_auto_discovery_queue.h"

DECLARE_ENUM(MQTTQueueEnum_t, uint8_t,
    SUBSCRIBE = 0,
    UNSUBSCRIBE,
    PUBLISH,
);

class MQTTPersistantStorageComponent;
class MQTTPlugin;

class MQTTClient {
public:
    using MQTTQueueType = MQTTQueueEnum_t;
    using MQTTComponentPtr = MQTTComponent::Ptr;
    using MQTTComponentVector = MQTTComponent::Vector;

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
        MQTTQueue(MQTTQueueType type, MQTTComponent *component, const String &topic, uint8_t qos, bool retain, const String &payload) :
            _type(type),
            _component(component),
            _topic(topic),
            _qos(qos),
            _retain(retain),
            _payload(payload) {
        }
        inline MQTTQueueType getType() const {
            return _type;
        }
        inline MQTTComponent *getComponent() const {
            return _component;
        }
        inline const String &getTopic() const {
            return _topic;
        }
        inline uint8_t getQos() const {
            return _qos;
        }
        inline bool getRetain() const {
            return _retain;
        }
        inline const String &getPayload() const {
            return _payload;
        }
    private:
        MQTTQueueType _type;
        MQTTComponent *_component;
        String _topic;
        uint8_t _qos: 2;
        uint8_t _retain: 1;
        String _payload;
    };

    typedef std::vector<MQTTTopic> MQTTTopicVector;
    typedef std::vector<MQTTQueue> MQTTQueueVector; // this is not used for QoS at the moment

    MQTTClient();
    virtual ~MQTTClient();

    void connect();
    void disconnect(bool forceDisconnect = false);
    void setLastWill(char value = 0);
    bool isConnected() const;
    void setAutoReconnect(uint32_t timeout);

    void registerComponent(MQTTComponentPtr component);
    bool unregisterComponent(MQTTComponentPtr component);
    bool isComponentRegistered(MQTTComponentPtr component);

    // <mqtt_topic=home/${device_name}>/component_name><format>
    // NOTE: there is no slash between the component name and format
    static String formatTopic(const String &componentName, const __FlashStringHelper *format = nullptr, ...);
private:
    static String _formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg);

public:
    void subscribe(MQTTComponentPtr component, const String &topic, uint8_t qos);
    void unsubscribe(MQTTComponentPtr component, const String &topic);
    void remove(MQTTComponentPtr component);
    void publish(const String &topic, uint8_t qos, bool retain, const String &payload);

    // return values
    // 0: failed to send
    // -1: success, but nothing was sent (in particular for unsubscribe if another component is still subscribed to the same topic)
    // for qos 1 and 2: >= 1 packet id, confirmation will be received by callbacks once delivered
    // for qos 0: 1 = successfully delivered to tcp stack
    int subscribeWithId(MQTTComponentPtr component, const String &topic, uint8_t qos);
    int unsubscribeWithId(MQTTComponentPtr component, const String &topic);
    int publishWithId(const String &topic, uint8_t qos, bool retain, const String &payload);

    static void setupInstance();
    static void deleteInstance();

    inline static void handleWiFiEvents(uint8_t event, void *payload) {
        _mqttClient->_handleWiFiEvents(event, payload);
    }

    static MQTTClient *getClient() {
        return _mqttClient;
    }

    static void safePublish(const String &topic, bool retain, const String &value) {
        if (_mqttClient) {
            _mqttClient->publish(topic, _mqttClient->_config.qos, retain, value);
        }
    }

    static void safeRegisterComponent(MQTTComponentPtr component) {
        if (_mqttClient) {
            _mqttClient->registerComponent(component);
        }
    }

    static bool safeUnregisterComponent(MQTTComponentPtr component) {
        if (_mqttClient) {
            return _mqttClient->unregisterComponent(component);
        }
        return false;
    }

    static void safeReRegisterComponent(MQTTComponentPtr component) {
        if (_mqttClient) {
            _mqttClient->unregisterComponent(component);
            _mqttClient->registerComponent(component);
        }
    }


    static uint8_t getDefaultQos() {
        if (_mqttClient) {
            return _mqttClient->_config.qos;
        }
        return config._H_GET(Config().mqtt.config).qos;
    }

    String connectionDetailsString();
    String connectionStatusString();

    void _handleWiFiEvents(uint8_t event, void *payload);
    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len);

#if MQTT_USE_PACKET_CALLBACKS
    void onPublish(uint16_t packetId);
    void onSubscribe(uint16_t packetId, uint8_t qos);
    void onUnsubscribe(uint16_t packetId);
#endif

private:
    void _setupClient();
    void autoReconnect(uint32_t timeout);

    const __FlashStringHelper *_reasonToString(AsyncMqttClientDisconnectReason reason) const;

    // match wild cards
    bool _isTopicMatch(const char *topic, const char *match) const;
    // check if the topic is in use by another component
    bool _topicInUse(MQTTComponentPtr component, const String &topic);

private:
    // if subscribe/unsubscribe/publish fails cause of the tcp client's buffer being full, retry later
    // the payload can be quite large for auto discovery for example
    // messages might be delivered in a different sequence. use subscribe/unsubscribe/publishWithId for full control
    // to deliver messages in sequence, use QoS and callbacks (onPublish() etc... requires MQTT_USE_PACKET_CALLBACKS=1)
    void _addQueue(MQTTQueueEnum_t type, MQTTComponent *component, const String &topic, uint8_t qos, uint8_t retain, const String &payload);
    // stop timer and clear queue
    void _clearQueue();
    // process queue
    void _queueTimerCallback();

    MQTTQueueVector _queue;
    EventScheduler::Timer _queueTimer;

private:
    friend MQTTAutoDiscoveryQueue;
    friend MQTTPersistantStorageComponent;
    friend MQTTPlugin;

    size_t getClientSpace() const;
    static bool _isMessageSizeExceeded(size_t len, const char *topic);

    String _host;
    String _username;
    String _password;
    Config_MQTT::config_t _config;
    AsyncMqttClient *_client;
    EventScheduler::Timer _timer;
    uint32_t _autoReconnectTimeout;
    uint16_t _maxMessageSize;
    uint16_t _componentsEntityCount;
    MQTTComponentVector _components;
    MQTTTopicVector _topics;
    Buffer _buffer;
    String _lastWillTopic;
    String _lastWillPayload;
    std::unique_ptr<MQTTAutoDiscoveryQueue> _autoDiscoveryQueue;

    static MQTTClient *_mqttClient;
};
