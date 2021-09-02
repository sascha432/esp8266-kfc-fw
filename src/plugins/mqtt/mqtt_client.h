/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <AsyncMqttClient.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <FixedString.h>
#include <vector>
#include "mqtt_base.h"
#include "EnumBase.h"
#include "kfc_fw_config.h"
#include "component.h"
#include "auto_discovery.h"
#include "auto_discovery_queue.h"
#include "component_proxy.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace MQTT {

    class QueueVector : public std::vector<ClientQueue>
    {
    public:
        using std::vector<ClientQueue>::vector;

        void clear() {
            vector::clear();
            _timer.remove();
        }

        iterator erase(iterator first, iterator last) {
            auto result = vector::erase(first, last);
            if (empty()) {
                removeTimer();
            }
            return result;
        }

        // erase all elements before the iterator (begin - prev(after))
        void eraseBefore(iterator after) {
            if (after == begin()) {
                return;
            }
            erase(begin(), prev(after));
        }

        void removeTimer() {
            _timer.remove();
        }

        Event::Timer &getTimer() {
            return _timer;
        }

    private:
        Event::Timer _timer;
    };

    class PacketQueueVector : public std::vector<PacketQueue>
    {
    public:
        using std::vector<PacketQueue>::vector;

        void clear() {
            vector::clear();
            _timer.remove();
            _internalPacketQueueId = 1;
        }

        template<typename _Ta>
        PacketQueue *find(_Ta value) {
            auto iterator = std::find(begin(), end(), value);
            if (iterator == end()) {
                return nullptr;
            }
            return &(*iterator);
        }

        bool setTimeout(uint16_t internalId, uint16_t timeout);

        iterator erase(iterator first, iterator last) {
            auto result = vector::erase(first, last);
            if (empty()) {
                removeTimer();
            }
            return result;
        }

        void removeTimer() {
            _timer.remove();
        }

        Event::Timer &getTimer() {
            return _timer;
        }

        uint16_t getNextPacketId() {
            if (++_internalPacketQueueId == (1U << 15)) { // ids are 15bit, 1-32767. 0 = invalid
                _internalPacketQueueId = 1;
            }
            return _internalPacketQueueId;
        }

    private:
        Event::Timer _timer;
        uint16_t _internalPacketQueueId;
    };

    class ClientTopic {
    public:
        ClientTopic(const String &topic, ComponentPtr component);

        const String &getTopic() const;
        ComponentPtr getComponent() const;
        bool match(ComponentPtr component, const String &topic) const;
        bool match(const String &topic) const;

    private:
        String _topic;
        ComponentPtr _component;
    };

    inline ClientTopic::ClientTopic(const String &topic, ComponentPtr component) :
        _topic(topic),
        _component(component)
    {
    }

    inline const String &ClientTopic::getTopic() const
    {
        return _topic;
    }

    inline ComponentPtr ClientTopic::getComponent() const
    {
        return _component;
    }

    inline bool ClientTopic::match(ComponentPtr component, const String &topic) const
    {
        return (component == _component) && (topic == _topic);
    }

    inline bool ClientTopic::match(const String &topic) const
    {
        return (topic == _topic);
    }

    class ClientQueue {
    public:
        ClientQueue(QueueType type, ComponentPtr component, uint16_t internalPacketId, const String &topic, QosType qos, bool retain, const String &payload, uint32_t time, uint16_t timeout);

        QueueType getType() const;
        ComponentPtr getComponent() const;
        const String &getTopic() const;
        QosType getQos() const;
        bool getRetain() const;
        const String &getPayload() const;

        bool isExpired() const;
        uint16_t getTimeout() const;

        uint16_t getInternalPacketId() const;
        void setInternalPacketId(uint16_t id);
        size_t getRequiredSize() const;

    private:
        ComponentPtr _component;
        String _topic;
        String _payload;
        uint32_t _time;
        uint16_t _timeout;
        uint16_t _internalPacketId;
        QueueType _type;
        QosType _qos;
        bool _retain;
    };

    static constexpr size_t ClientQueueSize = sizeof(ClientQueue);

    inline ClientQueue::ClientQueue(QueueType type, ComponentPtr component, uint16_t internalPacketId, const String &topic, QosType qos, bool retain, const String &payload, uint32_t time, uint16_t timeout) :
        _component(component),
        _topic(topic),
        _payload(payload),
        _time(time),
        _timeout(timeout),
        _internalPacketId(internalPacketId),
        _type(type),
        _qos(qos),
        _retain(retain)
    {
    }

    inline size_t ClientQueue::getRequiredSize() const
    {
        return _topic.length() + _payload.length() + 64;
    }

    inline QueueType ClientQueue::getType() const
    {
        return _type;
    }

    inline ComponentPtr ClientQueue::getComponent() const
    {
        return _component;
    }

    inline const String &ClientQueue::getTopic() const
    {
        return _topic;
    }

    inline QosType ClientQueue::getQos() const
    {
        return _qos;
    }

    inline bool ClientQueue::getRetain() const
    {
        return _retain;
    }

    inline const String &ClientQueue::getPayload() const
    {
        return _payload;
    }

    inline bool ClientQueue::isExpired() const
    {
        return get_time_diff(_time, millis()) > _timeout;
    }

    inline uint16_t ClientQueue::getTimeout() const
    {
        return _timeout;
    }

    inline uint16_t ClientQueue::getInternalPacketId() const
    {
        return _internalPacketId;
    }

    inline void ClientQueue::setInternalPacketId(uint16_t id)
    {
        _internalPacketId = id;
    }

    // helper for std::find
    class PacketQueueId {
    public:
        inline __attribute__((__always_inline__))
        PacketQueueId(uint16_t id) : _id(id) {}

        inline __attribute__((__always_inline__))
        uint16_t getId() const {
            return _id;
        }
    private:
        uint16_t _id;
    };

    // helper for std::find
    class PacketQueueInternalId {
    public:
        inline __attribute__((__always_inline__))
        PacketQueueInternalId(uint16_t id) : _id(id) {}

        inline __attribute__((__always_inline__))
        uint16_t getId() const {
            return _id;
        }
    private:
        uint16_t _id;
    };

    class PacketQueue {
    public:
        PacketQueue() : _packetId(0), _internalId(0), _delivered(false), _time(0), _timeout(kDefaultAckTimeout) {}
        // internalPacketId = generate id automatically
        PacketQueue(uint16_t packetId, uint32_t time, bool delivered = false, uint16_t internalPacketId = 0);

        inline __attribute__((__always_inline__))
        bool operator==(PacketQueueId packetId) const {
            return _packetId == packetId.getId();
        }

        inline __attribute__((__always_inline__))
        bool operator==(PacketQueueInternalId internalId) const {
            return _internalId == internalId.getId();
        }

        // alias for isValid()
        operator bool() const;

        // invalid packet, discard
        bool isValid() const;
        // packet was sent to the TCP stack
        bool isSent() const;

        void setDelivered();
        bool isDelivered() const;
        // check if the packet has been delivered in time
        bool isTimeout() const;
        uint16_t getInternalId() const;

        void update(uint16_t packetId, uint32_t time);
        void setTimeout(uint16_t timeout);
        uint16_t getTimeout() const;

        // debug
        String toString() const {
            return PrintString(F("packet_id=%u internal=%u delivered=%u timeout=%u/%u can_delete=%u"), _packetId, _internalId, isDelivered(), isTimeout(), getTimeout(), canDelete());
        }

    private:
        friend Client;

        // remove packet from packet queue
        // packets are kept 5 times longer than the ack. timeout
        // after that they report as not found instead of timeout
        bool canDelete() const;
        uint16_t getPacketId() const;

        // sending failed, add to queue
        bool _addToLocalQueue() const;

    private:
        struct __attribute__((packed)) {
            uint16_t _packetId;
            uint16_t _internalId: 15;
            uint16_t _delivered: 1;
            uint32_t _time;
            uint16_t _timeout;
        };
    };

    static constexpr size_t PacketQueueSize = sizeof(PacketQueue);

    inline __attribute__((__always_inline__))
    PacketQueue::operator bool() const
    {
        return _internalId != 0;
    }

    inline __attribute__((__always_inline__))
    bool PacketQueue::isValid() const
    {
        return _internalId != 0;
    }

    inline __attribute__((__always_inline__))
    bool PacketQueue::isSent() const
    {
        return _internalId && _packetId;
    }

    inline __attribute__((__always_inline__))
    void PacketQueue::setDelivered()
    {
        _delivered = true;
    }

    inline __attribute__((__always_inline__))
    bool PacketQueue::isDelivered() const
    {
        return _delivered;
    }

    inline bool PacketQueue::isTimeout() const
    {
        return !isDelivered() && get_time_diff(_time, millis()) >= _timeout;
    }

    inline __attribute__((__always_inline__))
    void PacketQueue::update(uint16_t packetId, uint32_t time)
    {
        _packetId = packetId;
        _time = time;
    }

    inline __attribute__((__always_inline__))
    void PacketQueue::setTimeout(uint16_t timeout)
    {
        _timeout = timeout;
    }

    inline __attribute__((__always_inline__))
    uint16_t PacketQueue::getTimeout() const
    {
        return _timeout;
    }

    inline bool PacketQueue::canDelete() const
    {
        auto diff = get_time_diff(_time, millis());
        return isDelivered() && (diff >= _timeout) && (diff >= kDefaultCanDeleteTimeout);
    }

    inline __attribute__((__always_inline__))
    uint16_t PacketQueue::getPacketId() const
    {
        return _packetId;
    }

    inline __attribute__((__always_inline__))
    uint16_t PacketQueue::getInternalId() const
    {
        return _internalId;
    }

    inline __attribute__((__always_inline__))
    bool PacketQueue::_addToLocalQueue() const
    {
        return !_packetId && _internalId && !isDelivered();
    }

    class Client {
    public:
        friend ComponentProxy;
        friend AutoDiscovery::Queue;
        friend PersistantStorageComponent;
        friend Plugin;

    public:
        Client();
        virtual ~Client();

        static void setupInstance();
        static void deleteInstance();

        // components must be registered to receive events and publish auto discovery
        static void registerComponent(ComponentPtr component);
        // unregistering removes all queued messages and unsubscribes from any topics
        static bool unregisterComponent(ComponentPtr component);
        // check if a component is registered
        static bool isComponentRegistered(ComponentPtr component);

        // return current client or nullptr if it does not exist
        static Client *getClient();

        // the safe methods verify that the client exists
        static void safePersistantStorage(StorageFrequencyType type, const String &name, const String &data);
#if MQTT_AUTO_DISCOVERY
        static bool safeIsAutoDiscoveryRunning();
#endif
        static bool safeIsConnected();

private:
        void _registerComponent(ComponentPtr component);
        bool _unregisterComponent(ComponentPtr component);
        void remove(ComponentPtr component);

public:
        // connect to server if not connected/connecting
        void connect();
        // disconnect from server
        void disconnect(bool forceDisconnect = false);
#if MQTT_SET_LAST_WILL_MODE != 0
        // publish last will
        void publishLastWill();
#endif
        // returns true for ConnectionState::CONNECTED,
        bool isConnected() const;
        // returns true for ConnectionState::PRE_CONNECT, CONNECTING and CONNECTED
        bool isConnecting() const;
        // set time to wait until next reconnect attempt. 0 = disable auto reconnect
        void setAutoReconnect(AutoReconnectType timeout);
        // timeout = minimum value or 0
        void setAutoReconnect();

        // <base_topic=home/${device_name}>/<component_name><format>
        // NOTE: there is no slash added between the component name and format
        static String formatTopic(const String &componentName, const __FlashStringHelper *format = nullptr, ...);
        // <base_topic=home/${device_name}>/<format>
        static String formatTopic(const __FlashStringHelper *format = nullptr, ...);

    public:
    #if MQTT_GROUP_TOPIC
    #error out of date
        void subscribeWithGroup(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
        void unsubscribeWithGroup(ComponentPtr component, const String &topic);
    #endif

#if MQTT_AUTO_DISCOVERY

    public:
        // force starts to send the auto discovery ignoring the delay between each run
        // returns false if running
        // abort = true aborts it if running
        // forceUpdate = true deletes all retained messages and sends new ones
        bool publishAutoDiscovery(RunFlags runFLags = RunFlags::DEFAULTS, AutoDiscovery::StatusCallback callback = nullptr);
        static void publishAutoDiscoveryCallback(Event::CallbackTimerPtr timer);

        inline bool isAutoDiscoveryRunning() const {
            return static_cast<bool>(_autoDiscoveryQueue);
        }

        // returns a list of all auto discovery components
        inline AutoDiscovery::List getAutoDiscoveryList(FormatType format = FormatType::JSON) {
            return AutoDiscovery::List(_components, format);
        }

        AutoDiscovery::QueuePtr &getAutoDiscoveryQueue();
        // additional information about the current message
        // must not be called outside onMessage()

        inline const AsyncMqttClientMessageProperties &getMessageProperties() const {
#if DEBUG_MQTT_CLIENT
            if (!_messageProperties) {
                __DBG_panic("getMessageProperties() must not be called outside onMessage()");
            }
#endif
            return *_messageProperties;
        }

        static inline String _getAutoDiscoveryStatusTopic() {
            return formatTopic(F("/auto_discovery/state"));
        }

        void updateAutoDiscoveryTimestamps(bool success);

        // once set, the time is not updated from MQTT anymore
        inline void _resetAutoDiscoveryInitialState() {
            if (_autoDiscoveryLastSuccess == ~0U) {
                _autoDiscoveryLastSuccess = 0;
            }
            if (_autoDiscoveryLastFailure == ~0U) {
                _autoDiscoveryLastFailure = 0;
            }
        }

        inline bool isAutoDiscoveryLastTimeValid() const {
            return (_autoDiscoveryLastSuccess != ~0U && _autoDiscoveryLastFailure != ~0U);
        }

        String  _autoDiscoveryStatusTopic;
        uint32_t _autoDiscoveryLastFailure{~0U};
        uint32_t _autoDiscoveryLastSuccess{~0U};

#endif

    public:
        // returns internal packet id for tracking
        // if QoS == 0, the internal id cannot be used to track the message
        // the message might be queued and not sent yet (TCP buffers full...)
        // 0 = failed to send (queue full, max. message size exceeded)
        uint16_t subscribe(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT);
        // unsubscribe will mute the topic for the component even if the unsubscribe message cannot be delivered
        uint16_t unsubscribe(ComponentPtr component, const String &topic);
        uint16_t publish(ComponentPtr component, const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);
        // publishing without component will keep the data in the queue when a component gets removed
        uint16_t publish(const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT);

        PacketQueue subscribeWithId(ComponentPtr component, const String &topic, QosType qos = QosType::DEFAULT, uint16_t internalPacketId = 0);
        // unsubscribe will mute the topic for the component even if the unsubscribe message cannot be delivered
        PacketQueue unsubscribeWithId(ComponentPtr component, const String &topic, uint16_t internalPacketId = 0);
        // component may be nullptr see publish()
        PacketQueue publishWithId(ComponentPtr component, const String &topic, bool retain, const String &payload, QosType qos = QosType::DEFAULT, uint16_t internalPacketId = 0);

        PacketQueue recordId(int packetId, QosType qos, uint16_t internalPacketId = 0);
        PacketQueue getQueueStatus(int packetId);
        static uint16_t getNextInternalPacketId();

        static void handleWiFiEvents(WiFiCallbacks::EventType event, void *payload);

    public:
        // returns 1 for: [ any integer != 0, "on", "yes", "enable", "enabled", SPGM(mqtt_bool_on)="ON", SPGM(mqtt_bool_true)="true", SPGM(mqtt_status_topic_online="online") ]
        // returns 0 for: [ 0, "00"..., "off", "no", "disable", "disabled", SPGM(mqtt_bool_off)="OFF", SPGM(mqtt_bool_false)="false", SPGM(mqtt_status_topic_offline)="offline" ]
        // otherwise it returns -1 or the value of "invalid"
        // white spaces are stripped and the strings are case insensitive
        static int8_t toBool(const char *, int8_t invalid = -1);
        static const __FlashStringHelper *toBoolTrueFalse(bool value);
        static const __FlashStringHelper *toBoolOnOff(bool value);

        static QosType getDefaultQos(QosType qos = QosType::DEFAULT);

        String connectionDetailsString();
        String connectionStatusString();

        void _handleWiFiEvents(WiFiCallbacks::EventType event, void *payload);

        // there is no session or any data keep between reconnects
        void onConnect(bool sessionPresent);
        void onDisconnect(AsyncMqttClientDisconnectReason reason);

        void onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
        void onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len);

        TopicVector &getTopics();
        size_t getQueueSize() const;

    public:
        // return qos if not QosType::DEFAULT, otherwise return the qos value from the configuration
        static QosType _getDefaultQos(QosType qos = QosType::DEFAULT);
        // translate into 0, 1 or 2
        static uint8_t _translateQosType(QosType qos = QosType::DEFAULT);

        #if MQTT_AUTO_DISCOVERY_USE_NAME
            static String getAutoDiscoveryName(const __FlashStringHelper *componentName);
        #endif

    private:
        static String _getBaseTopic();
    #if MQTT_GROUP_TOPIC
        static bool _getGroupTopic(ComponentPtr component, String groupTopic, String &topic);
    #endif
        // a slash is inserted between base topic and suffix or format, if there is none
        // there is no slash inserted between suffix and format
        static String _formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg);
        static String _filterString(const char *str, bool replaceSpace = false);

    public:
        inline const __FlashStringHelper *getConnectionState()
        {
            switch(_connState) {
                case ConnectionState::AUTO_RECONNECT_DELAY:
                    return F("waiting for reconnect timeout");
                case ConnectionState::PRE_CONNECT:
                case ConnectionState::CONNECTING:
                    return F("connecting...");
                case ConnectionState::CONNECTED:
                    return F("connected");
                case ConnectionState::DISCONNECTING:
                    return F("disconnecting...");
                case ConnectionState::DISCONNECTED:
                case ConnectionState::IDLE:
                case ConnectionState::NONE:
                default:
                    break;
            }
            return F("disconnected");
        }

    private:
        void _zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type);
        void _setupClient();
        void autoReconnect(AutoReconnectType timeout);

        NameType _reasonToString(AsyncMqttClientDisconnectReason reason) const;
        StringVector _createAutoDiscoveryTopics() const;

    public:
        // match wild cards
        bool _isTopicMatch(const char *topic, const char *match) const;

    private:
        // check if the topic is in use by another component
        inline __attribute__((__always_inline__))
        bool _topicInUse(ComponentPtr component, const ClientTopic &topic) {
            return _topicInUse(component, topic.getTopic());
        }
        bool _topicInUse(ComponentPtr component, const String &topic);

    private:
        void _resetClient();

    private:
        // delivery and packet queue

        // if subscribe/unsubscribe/publish fails cause of the tcp client's buffer being full, the message is added to the local queue
        // messages are sent in order and discarded after a timeout
        void _addQueue(QueueType type, ComponentPtr component, PacketQueue &queue, const String &topic, QosType qos, bool retain = false, const String &payload = String(), uint16_t timeout = kDefaultQueueTimeout);
        // start timer for the delivery queue
        void _queueStartTimer();
        // process delivery queue
        void _queueTimerCallback();
        // subscribe, unsubscribe, publish
        void _onPacketAck(uint16_t packetId, PacketAckType type);
        // timeout, error
        void _onErrorPacketAck(uint16_t internalId, PacketAckType type);
        // start timer for the packet queue
        void _packetQueueStartTimer();
        // process timeouts of the packet queue
        void _packetQueueTimerCallback();

    private:

        QueueVector _queue;
        PacketQueueVector _packetQueue;

    private:
        size_t getClientSpace() const;
        static bool _isMessageSizeExceeded(size_t len, const char *topic);
        ConnectionState setConnState(ConnectionState newState);

    private:
        String _hostname;
        IPAddress _address;
        String _username;
        String _password;
        MqttConfigType _config;
        AsyncMqttClient *_client;
        Event::Timer _timer;
        uint16_t _port;
#if MQTT_SET_LAST_WILL_MODE != 0
        // these variables are only required if last will is being used since the MQTT client requires a
        // pointer to memory and does not support flash strings
        const String _lastWillPayloadOffline;
        const String _lastWillTopic;
#endif
        AutoReconnectType _autoReconnectTimeout;
        TopicVector _topics;
        Buffer _buffer;
        ConnectionState _connState;
#if MQTT_AUTO_DISCOVERY
        AutoDiscovery::QueuePtr _autoDiscoveryQueue;
        Event::Timer _autoDiscoveryRebroadcast;
        bool _startAutoDiscovery;
#endif
        AsyncMqttClientMessageProperties *_messageProperties;

    #if DEBUG_MQTT_CLIENT
    public:
        NameType _getConnStateStr();
        const char *_connection();
        String _connectionStr;
    #else
    const char *_connection() { return PSTR("NO_DEBUG"); }
    #endif

    private:
        static ComponentVector _components;
        static Client *_mqttClient;
        MutexSemaphore _lock;
    };


    inline void Client::setAutoReconnect(AutoReconnectType timeout)
    {
        __LDBG_printf("timeout=%d conn=%s", timeout, _connection());
        _autoReconnectTimeout = timeout;
    }

    inline __attribute__((__always_inline__))
    void Client::setAutoReconnect()
    {
        setAutoReconnect(_config.auto_reconnect_min);
    }

    inline __attribute__((__always_inline__))
    bool Client::isConnected() const
    {
        return _connState == ConnectionState::CONNECTED;
    }

    inline __attribute__((__always_inline__))
    size_t Client::getClientSpace() const
    {
        return _client->getAsyncClient().space();
    }

    inline ConnectionState Client::setConnState(ConnectionState newState)
    {
        auto tmp = _connState;
        _connState = newState;
        return tmp;
    }

    inline __attribute__((__always_inline__))
    TopicVector &Client::getTopics()
    {
        return _topics;
    }

    inline __attribute__((__always_inline__))
    size_t Client::getQueueSize() const
    {
        return _queue.size();
    }

    inline void Client::publishAutoDiscoveryCallback(Event::CallbackTimerPtr timer)
    {
        if (_mqttClient) {
            _mqttClient->publishAutoDiscovery();
        }
    }

    inline void Client::handleWiFiEvents(WiFiCallbacks::EventType event, void *payload)
    {
        if (_mqttClient) {
            _mqttClient->_handleWiFiEvents(event, payload);
        }
    }

    inline __attribute__((__always_inline__))
    Client *Client::getClient()
    {
        return _mqttClient;
    }

    inline bool Client::safeIsAutoDiscoveryRunning()
    {
        if (_mqttClient) {
            return _mqttClient->isAutoDiscoveryRunning();
        }
        return false;
    }

    inline bool Client::safeIsConnected()
    {
        if (_mqttClient) {
            return _mqttClient->isConnected();
        }
        return false;
    }

    inline __attribute__((__always_inline__))
    QosType Client::getDefaultQos(QosType qos)
    {
        return qos;
    }

    inline QosType Client::_getDefaultQos(QosType qos)
    {
        if (qos != QosType::DEFAULT) {
            return qos;
        }
        if (_mqttClient) {
            return static_cast<QosType>(_mqttClient->_config.qos);
        }
        return static_cast<QosType>(MqttClient::getConfig().qos);
    }

    inline __attribute__((__always_inline__))
    uint8_t Client::_translateQosType(QosType qos)
    {
        return static_cast<uint8_t>(_getDefaultQos(qos));
    }

    #if MQTT_AUTO_DISCOVERY_USE_NAME

    inline String Client::getAutoDiscoveryName(const __FlashStringHelper *componentName)
    {
        String name = KFCConfigurationClasses::Plugins::MQTTConfigNS::MqttClient::getAutoDiscoveryName();
        if (name.length() == 0) {
            return String();
        }
        if (name.indexOf(F("${device_name}")) != -1) {
            name.replace(F("${device_name}"), KFCConfigurationClasses::System::Device::getName());
        }
        if (name.indexOf(F("${device_title}")) != -1) {
            name.replace(F("${device_title}"), KFCConfigurationClasses::System::Device::getTitle());
        }
        name.replace(F("${component_name}"), componentName);
        return name;
    }

    #endif

    inline __attribute__((__always_inline__))
    AutoDiscovery::QueuePtr &Client::getAutoDiscoveryQueue() {
        return _autoDiscoveryQueue;
    }

    inline __attribute__((__always_inline__))
    const __FlashStringHelper *Client::toBoolTrueFalse(bool value)
    {
        return value ? FSPGM(mqtt_bool_true) : FSPGM(mqtt_bool_false);
    }

    inline __attribute__((__always_inline__))
    const __FlashStringHelper *Client::toBoolOnOff(bool value)
    {
        return value ? FSPGM(mqtt_bool_on) : FSPGM(mqtt_bool_off);
    }

    inline __attribute__((__always_inline__))
    uint16_t Component::subscribe(const String& topic, QosType qos)
    {
        if (!isConnected()) {
            __DBG_assert_printf(isConnected() == true, "isConnected() == false: subscribe(%s) client=%p", __S(topic), getClient());
            return 0;
        }
        return client().subscribe(this, topic, qos);
    }

    inline __attribute__((__always_inline__))
    uint16_t Component::unsubscribe(const String& topic)
    {
        if (!isConnected()) {
            __DBG_assert_printf(isConnected() == true, "isConnected() == false: unsubscribe(%s) client=%p", __S(topic), getClient());
            return 0;
        }
        return client().unsubscribe(this, topic);
    }

    inline __attribute__((__always_inline__))
    uint16_t Component::publish(const String& topic, bool retain, const String& payload, QosType qos)
    {
        if (!isConnected()) {
            __DBG_assert_printf(isConnected() == true, "isConnected() == false: publish(%s) client=%p", __S(topic), getClient());
            return 0;
        }
        return client().publish(this, topic, retain, payload, qos);
    }

    inline __attribute__((__always_inline__))
    bool ComponentBase::isConnected() const
    {
        return _client != nullptr && _client->isConnected();
    }
}

#include <debug_helper_disable.h>
