/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#ifndef DEBUG_MQTT_CLIENT
#define DEBUG_MQTT_CLIENT 1
#endif

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <vector>
#include "mqtt_component.h"

typedef struct {
    String topic;
    MQTTComponent *component;
} MQTTTopic_t;

typedef std::vector<MQTTComponent *> MQTTComponentVector;
typedef std::vector<MQTTTopic_t> MQTTTopicVector;

class MQTTClient {
public:
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

    bool hasMultipleComponments() const;
    static const String getComponentName(uint8_t num = -1);
    static String formatTopic(uint8_t num, const __FlashStringHelper *format, ...);

    void subscribe(MQTTComponent *component, const String &topic, uint8_t qos);
    void unsubscribe(MQTTComponent *component, const String &topic);
    void publish(const String &topic, uint8_t qos, bool retain, const String &payload);

    static void setup();
    static const String connectionDetailsString();
    static const String connectionStatusString();
    static const String getStatus();
    static MQTTClient *getClient();
    static void handleWiFiEvents(uint8_t event, void *payload);
    static uint8_t getDefaultQos() {
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
    const String _createHASSYaml();

private:
    AsyncMqttClient *_client;
    EventScheduler::TimerPtr _timer;
    uint32_t _autoReconnectTimeout;
    uint16_t _maxMessageSize;
    MQTTComponentVector _components;
    MQTTTopicVector _topics;
    Buffer *_messageBuffer;
    String _lastWillTopic;
};

#endif
