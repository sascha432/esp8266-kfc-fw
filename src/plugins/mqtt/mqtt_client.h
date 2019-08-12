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
#define MQTT_AUTO_DISCOVERY                 0
#endif

// run auto discovery client on device and provide additional AT commands. needs a lot memory when active
// Enabled/disable with +MMQTAD=1/0
#ifndef MQTT_AUTO_DISCOVERY_CLIENT
#if MQTT_AUTO_DISCOVERY && AT_MODE_SUPPORTED
#define MQTT_AUTO_DISCOVERY_CLIENT          1
#endif
#endif

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <vector>
#include "kfc_fw_config.h"
#include "mqtt_component.h"

class MQTTClient {
public:
    typedef struct {
        String topic;
        MQTTComponent *component;
    } MQTTTopic_t;

    typedef enum : uint8_t {
        QUEUE_SUBSCRIBE = 0,
        QUEUE_UNSUBSCRIBE,
        QUEUE_PUBLISH,
    } MQTTQueueEnum_t;

    typedef struct {
        MQTTQueueEnum_t type;
        String topic;
        uint8_t qos: 2;
        uint8_t retain: 1;
        String payload;
    } MQTTQueue_t;

    typedef std::vector<MQTTComponent *> MQTTComponentVector;
    typedef std::vector<MQTTTopic_t> MQTTTopicVector;
    typedef std::vector<MQTTQueue_t> MQTTQueueVector; // this is not used for QoS at the moment

    static const int DEFAULT_RECONNECT_TIMEOUT = 5000;
    static const int MAX_MESSAGE_SIZE = 1024;

    MQTTClient();
    virtual ~MQTTClient();

    void connect();
    void disconnect(bool forceDisconnect = false);
    bool isConnected() const;
    void setAutoReconnect(uint32_t timeout);

    void registerComponent(MQTTComponent *component);
    void unregisterComponent(MQTTComponent *component);

    inline bool hasMultipleComponments() const {
        return _components.size() > 1;
    }
    inline bool useNodeId() const {
        return _useNodeId;
    }
    // false <discovery_prefix>/<component>/<name>_<node_id>
    // true <discovery_prefix>/<component>/<name>/<node_id>
    void setUseNodeId(bool useNodeId) {
        _useNodeId = useNodeId;
    }

    static const String getComponentName(uint8_t num = -1);
    static String formatTopic(uint8_t num, const __FlashStringHelper *format, ...);

    void subscribe(MQTTComponent *component, const String &topic, uint8_t qos);
    void unsubscribe(MQTTComponent *component, const String &topic);
    void remove(MQTTComponent *component);

    void publish(const String &topic, uint8_t qos, bool retain, const String &payload);

    static void setupInstance();
    static void deleteInstance();

    static const String connectionDetailsString();
    static const String connectionStatusString();
    static const String getStatus();
    static void handleWiFiEvents(uint8_t event, void *payload);

    inline static MQTTClient *getClient() {
        return _mqttClient;
    }
   
    inline static uint8_t getDefaultQos() {
        return config._H_GET(Config().mqtt_qos);
    }

    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len);
    void onPublish(uint16_t packetId);
    void onSubscribe(uint16_t packetId, uint8_t qos);
    void onUnsubscribe(uint16_t packetId);

private:
    void autoReconnect(uint32_t timeout);

    bool _isTopicMatch(const char *topic, const char *match) const;
    const String _reasonToString(AsyncMqttClientDisconnectReason reason) const;
    // check if the topic is in use by another component
    bool _topicInUse(MQTTComponent *component, const String &topic);
    // if subscribe/unsubscribe/publush fails cause of the tcp client's buffer being full, retry later. the payload can be quite large for auto discovery
    void _addQueue(MQTTQueue_t queue);
    // stop timer and clear queue
    void _clearQueue();
    // process queue
    void _queueTimerCallback(EventScheduler::TimerPtr timer);
    
public:
    static void queueTimerCallback(EventScheduler::TimerPtr timer);

private:
    AsyncMqttClient *_client;
    EventScheduler::TimerPtr _timer;
    uint32_t _autoReconnectTimeout;
    uint16_t _maxMessageSize;
    uint8_t _useNodeId: 1;
    MQTTComponentVector _components;
    MQTTTopicVector _topics;
    MQTTQueueVector _queue;
    EventScheduler::TimerPtr _queueTimer;
    Buffer *_messageBuffer;
    String _lastWillTopic;

    static MQTTClient *_mqttClient;
};

#endif
