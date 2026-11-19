/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <EventScheduler.h>
#include <vector>
#include "mqtt_base.h"
#include "kfc_fw_config.h"
#include "component.h"
#include "auto_discovery.h"
#include "discovery.h"

namespace MQTT {

    // a message or event in one heap block: the header, the topic and the payload, each NUL terminated
    struct Packet {
        enum class Type : uint8_t {
            // outgoing
            PUBLISH,
            SUBSCRIBE,
            UNSUBSCRIBE,
            // incoming
            MESSAGE,
            DISCOVERY_TOPIC,    // retained auto discovery topic of this device, the payload is not stored
            CONNECTED,
            DISCONNECTED,       // value = reason
            ACK,                // value = packet id
            WIFI_UP,
            WIFI_DOWN,
        };

        Packet *next;
        uint16_t topicLength;
        uint16_t payloadLength;
        uint16_t value;
        Type type;
        uint8_t qos: 2;
        uint8_t retain: 1;

        char *topic() {
            return reinterpret_cast<char *>(this + 1);
        }

        char *payload() {
            return topic() + topicLength + 1;
        }

        size_t size() const {
            return sizeof(Packet) + topicLength + payloadLength + 2;
        }

        // payload nullptr leaves the payload uninitialized. returns nullptr if the memory cannot be allocated
        static Packet *create(Type type, const char *topic = nullptr, size_t topicLength = 0, const char *payload = nullptr, size_t payloadLength = 0, uint8_t qos = 0, bool retain = false, uint16_t value = 0);
    };

    // FIFO of packets. on ESP32 the producer can be another task (spinlock around the pointer operations, no
    // allocation inside), on ESP8266 the library callbacks and the main loop do not preempt each other
    class PacketList {
    public:
        PacketList();
        ~PacketList();

        // false if the list would exceed maxSize, the packet is deleted. maxSize = 0 no limit
        // replace = true replaces a retained message to the same topic that has not been sent yet
        bool push(Packet *packet, size_t maxSize = 0, bool replace = false);
        Packet *pop();
        Packet *front() const {
            return _head;
        }
        void clear();

        size_t size() const {
            return _size;
        }

        uint16_t count() const {
            return _count;
        }

    private:
        Packet *_head;
        Packet *_tail;
        size_t _size;
        uint16_t _count;
        #if ESP32
            portMUX_TYPE _mux;
        #endif
    };

    struct Subscription {
        String topic;
        ComponentPtr component;

        Subscription(const String &aTopic, ComponentPtr aComponent) :
            topic(aTopic),
            component(aComponent)
        {
        }
    };

    using SubscriptionVector = std::vector<Subscription>;

    // the MQTT client. everything runs in the main loop, the callbacks of the MQTT library only add events to
    // the inbox
    //
    // IDLE         no WiFi, disabled or disconnected without reconnect
    // WAIT         waiting for the reconnect delay
    // CONNECTING   connect() called, waiting for the server to accept it
    // CONNECTED    connected, the outbox is sent and the auto discovery runs
    //
    // the outbox hands one packet at a time to the library and waits for its acknowledgment (QoS 1/2,
    // subscribe and unsubscribe) and for space in the TCP buffer
    class Client {
    public:
        enum class StateType : uint8_t {
            IDLE,
            WAIT,
            CONNECTING,
            CONNECTED,
        };

        Client();
        ~Client();

        static void setupInstance();
        static void deleteInstance();

        // nullptr if MQTT is disabled
        static Client *getClient() {
            return _client;
        }

        static bool safeIsConnected() {
            return _client && _client->isConnected();
        }

        static bool safeIsAutoDiscoveryRunning() {
            return _client && _client->isAutoDiscoveryRunning();
        }

        // components receive the connection events and messages and provide the auto discovery
        static void registerComponent(ComponentPtr component);
        // removes the component and its subscriptions
        static bool unregisterComponent(ComponentPtr component);
        static bool isComponentRegistered(ComponentPtr component);

        static ComponentVector &getComponents() {
            return _components;
        }

        bool isConnected() const {
            return _state == StateType::CONNECTED;
        }

        StateType getState() const {
            return _state;
        }

        // connect now and reset the reconnect delay
        void connect();
        // publish the offline status and disconnect. reconnect = false stops reconnecting until connect() is called
        void disconnect(bool reconnect);

        // subscribe and unsubscribe must be called by the main loop. they return false if not connected or the
        // outbox is full
        bool subscribe(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
        bool unsubscribe(ComponentPtr component, const String &topic);
        // can be called by any task. a retained message replaces one to the same topic that is still in the outbox
        bool publish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);

        // starts the auto discovery after a short delay. a running one starts again
        bool publishAutoDiscovery(AutoDiscovery::StatusCallback callback = nullptr);
        bool isAutoDiscoveryRunning() const;

        const SubscriptionVector &getSubscriptions() const {
            return _subscriptions;
        }

        // the short status of the WebUI (HTML)
        void printStatus(Print &output);
        // everything for the AT command
        void printDebug(Print &output);

    public:
        // <base_topic=home/${device_name}>/<component_name><format>
        // NOTE: there is no slash added between the component name and format
        static String formatTopic(const String &componentName, const __FlashStringHelper *format = nullptr, ...);
        // <base_topic=home/${device_name}>/<format>
        static String formatTopic(const __FlashStringHelper *format = nullptr, ...);
        static String getBaseTopic();
        // object id or device name with spaces replaced by underscores and lowercase, ends with "_"
        static String getBaseTopicPrefix();

        // replaceSpace = true replaces spaces with underscore and changes the name to lowercase, otherwise
        // removes anything that is not alphanumeric
        static String filterString(const char *str, bool replaceSpace = false);

        static String filterString(const String &str, bool replaceSpace = false) {
            return filterString(str.c_str(), replaceSpace);
        }

        static String filterString(const __FlashStringHelper *str, bool replaceSpace = false) {
            return filterString(String(str).c_str(), replaceSpace);
        }

        // match + and # wildcards
        static bool isTopicMatch(const char *topic, const char *match);

        // returns 1 for: [ any integer != 0, "on", "yes", "enable", "enabled", "ON", "true", "online" ]
        // returns 0 for: [ 0, "00"..., "off", "no", "disable", "disabled", "OFF", "false", "offline" ]
        // otherwise it returns -1 or the value of "invalid"
        // white spaces are stripped and the strings are case insensitive
        static int8_t toBool(const char *str, int8_t invalid = -1);

        static const __FlashStringHelper *toBoolTrueFalse(bool value) {
            return value ? FSPGM(mqtt_bool_true) : FSPGM(mqtt_bool_false);
        }

        static const __FlashStringHelper *toBoolOnOff(bool value) {
            return value ? FSPGM(mqtt_bool_on) : FSPGM(mqtt_bool_off);
        }

        static String toRGB(uint32_t color);

    private:
        friend AutoDiscovery::Discovery;

        static void _loopFunction();
        void _loop();
        void _processInbox();
        void _pump(uint32_t now);

        void _setServer(const String &hostname, const IPAddress &address, uint16_t port);
        void _connect(uint32_t now);
        void _onConnected();
        void _onDisconnected(AsyncMqttClientDisconnectReason reason);
        void _onMessage(Packet *packet);
        void _onWiFi(bool up);
        // the connection has been closed or lost: discard the outbox and subscriptions and call onDisconnect()
        void _closed(AsyncMqttClientDisconnectReason reason);
        void _scheduleReconnect();
        void _abort(const __FlashStringHelper *message);
        void _startDiscovery(uint32_t delayMillis, AutoDiscovery::StatusCallback callback);

        // adds a packet to the outbox
        bool _enqueue(Packet::Type type, const String &topic, const char *payload, size_t payloadLength, QosType qos, bool retain);
        uint8_t _getQos(QosType qos) const;
        void _printServer(Print &output) const;

        // callbacks of the MQTT library
        static void _pushEvent(Packet::Type type, uint16_t value = 0);
        static void _onMessageCallback(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

        static const __FlashStringHelper *_getReasonString(AsyncMqttClientDisconnectReason reason);
        static const __FlashStringHelper *_getStateString(StateType state);

    private:
        AsyncMqttClient _mqtt;
        MqttConfigType _config;
        String _hostname;
        String _username;
        String _password;
        String _lastWillTopic;
        String _lastWillPayload;
        IPAddress _address;
        uint16_t _port;
        PacketList _outbox;
        Packet *_current;               // removed from the outbox, waiting for space in the TCP buffer
        SubscriptionVector _subscriptions;
        std::unique_ptr<AutoDiscovery::Discovery> _discovery;
        AutoDiscovery::Stats _discoveryStats;
        uint32_t _deadline;             // WAIT: connect, CONNECTING: timeout
        uint32_t _reconnectDelay;
        uint32_t _ackTime;
        uint32_t _retryTime;
        uint32_t _connectedTime;
        uint16_t _ackId;                // packet waiting for an acknowledgment, 0 = none
        uint16_t _retries;
        uint16_t _dropped;              // messages that did not fit into the outbox
        StateType _state;
        bool _resolved;                 // the server address is known
        bool _reconnect;                // reconnect after the connection has been lost

        static Client *_client;
        static ComponentVector _components;
        static PacketList _inbox;
        static Packet *_rxPacket;       // message being received in chunks (library callback)
        static String _rxDiscoveryPrefix;
        static uint16_t _rxDropped;     // incoming messages discarded
        static uint8_t _generation;     // zeroconf callbacks of a previous client are ignored
    };

}
