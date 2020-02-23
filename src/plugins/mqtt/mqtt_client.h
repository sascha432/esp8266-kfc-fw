/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if MQTT_SUPPORT

#ifndef DEBUG_MQTT_CLIENT
#define DEBUG_MQTT_CLIENT                   0
#endif

// home assistant auto discovery
#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY                 1
#endif

// disable last will. status is set to offline when disconnecting only
#ifndef MQTT_SET_LAST_WILL
#define MQTT_SET_LAST_WILL                  1
#endif

// run auto discovery client on device and provide additional AT commands. needs a lot memory when active
// Enabled/disable with +MMQTAD=1/0
#ifndef MQTT_AUTO_DISCOVERY_CLIENT
#if MQTT_AUTO_DISCOVERY && AT_MODE_SUPPORTED
#define MQTT_AUTO_DISCOVERY_CLIENT          0
#endif
#endif

// enable callbacks for QoS. onSubscribe(), onPublish() and onUnsubscribe()
#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS           0
#endif

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <vector>
#include "EnumBase.h"
#include "kfc_fw_config.h"
#include "mqtt_component.h"

DECLARE_ENUM(MQTTQueueType, uint8_t,
    SUBSCRIBE = 0,
    UNSUBSCRIBE,
    PUBLISH,
);

class MQTTClient {
public:
    typedef MQTTQueueType MQTTQueueEnum_t;

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
        MQTTQueue(MQTTQueueType type, MQTTComponent *component, const String &topic, uint8_t qos, uint8_t retain, const String &payload) :
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
        inline uint8_t getRetain() const {
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

    typedef MQTTComponent* MQTTComponentPtr;
    typedef std::vector<MQTTComponentPtr> MQTTComponentVector;
    typedef std::vector<MQTTTopic> MQTTTopicVector;
    typedef std::vector<MQTTQueue> MQTTQueueVector; // this is not used for QoS at the moment

    static const int DEFAULT_RECONNECT_TIMEOUT = 5000;
    static const int MAX_MESSAGE_SIZE = 1024;

    MQTTClient();
    virtual ~MQTTClient();

    void connect();
    void disconnect(bool forceDisconnect = false);
    void setLastWill(char value = 0);
    bool isConnected() const;
    void setAutoReconnect(uint32_t timeout);

    void registerComponent(MQTTComponentPtr component);
    void unregisterComponent(MQTTComponentPtr component);

    bool hasMultipleComponments() const;

    bool useNodeId() const {
        return _useNodeId;
    }
    // false <discovery_prefix>/<component>/<name>_<node_id>
    // true <discovery_prefix>/<component>/<name>/<node_id>
    void setUseNodeId(bool useNodeId) {
        _useNodeId = useNodeId;
    }

    static const String getComponentName(uint8_t num = -1);
    static String formatTopic(uint8_t num, const __FlashStringHelper *format, ...);
    // static String formatTopic(uint8_t num, const char *format, ...);
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

    static const String connectionDetailsString();
    static const String connectionStatusString();
    static void getStatus(Print &output);
    static void handleWiFiEvents(uint8_t event, void *payload);

    inline static MQTTClient *getClient() {
        return _mqttClient;
    }

    inline static uint8_t getDefaultQos() {
        return config._H_GET(Config().mqtt.config).qos;
    }

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
    void _queueTimerCallback(EventScheduler::TimerPtr timer);

    MQTTQueueVector _queue;
    EventScheduler::Timer _queueTimer;

public:
    static void queueTimerCallback(EventScheduler::TimerPtr timer);

private:
    AsyncMqttClient *_client;
    EventScheduler::Timer _timer;
    uint32_t _autoReconnectTimeout;
    uint16_t _maxMessageSize;
    uint8_t _useNodeId: 1;
    MQTTComponentVector _components;
    MQTTTopicVector _topics;
    Buffer _buffer;
    String _lastWillTopic;
    String _lastWillPayload;

    static MQTTClient *_mqttClient;
};

#endif
