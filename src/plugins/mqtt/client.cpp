/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintString.h>
#include "mqtt_plugin.h"
#include "mqtt_client.h"
#include "logger.h"
#include "templates.h"

#if DEBUG_MQTT_CLIENT
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

#if ESP32
#    define PACKET_LIST_LOCK()      portENTER_CRITICAL(&_mux)
#    define PACKET_LIST_UNLOCK()    portEXIT_CRITICAL(&_mux)
#else
#    define PACKET_LIST_LOCK()
#    define PACKET_LIST_UNLOCK()
#endif

namespace MQTT {

    // maximum size of incoming messages waiting for the main loop
    static constexpr size_t kInboxMaxSize = MQTT_RECV_MAX_MESSAGE_SIZE * 4;
    // a message the library refuses (not enough memory) is dropped after this many attempts, 100 ms apart
    static constexpr uint16_t kMaxRetries = 300;

    static inline bool timeReached(uint32_t now, uint32_t time)
    {
        return static_cast<int32_t>(now - time) >= 0;
    }

    // ------------------------------------------------------------------------
    // Packet, PacketList
    // ------------------------------------------------------------------------

    Packet *Packet::create(Type type, const char *topic, size_t topicLength, const char *payload, size_t payloadLength, uint8_t qos, bool retain, uint16_t value)
    {
        auto packet = reinterpret_cast<Packet *>(malloc(sizeof(Packet) + topicLength + payloadLength + 2));
        if (!packet) {
            return nullptr;
        }
        packet->next = nullptr;
        packet->topicLength = topicLength;
        packet->payloadLength = payloadLength;
        packet->value = value;
        packet->type = type;
        packet->qos = qos;
        packet->retain = retain;
        if (topicLength) {
            memcpy(packet->topic(), topic, topicLength);
        }
        packet->topic()[topicLength] = 0;
        if (payload && payloadLength) {
            memcpy(packet->payload(), payload, payloadLength);
        }
        packet->payload()[payloadLength] = 0;
        return packet;
    }

    PacketList::PacketList() :
        _head(nullptr),
        _tail(nullptr),
        _size(0),
        _count(0)
    {
        #if ESP32
            portMUX_INITIALIZE(&_mux);
        #endif
    }

    PacketList::~PacketList()
    {
        clear();
    }

    bool PacketList::push(Packet *packet, size_t maxSize, bool replace)
    {
        Packet *replaced = nullptr;
        bool result = true;
        PACKET_LIST_LOCK();
        if (replace) {
            Packet *prev = nullptr;
            for (auto cur = _head; cur; prev = cur, cur = cur->next) {
                if (cur->type == packet->type && cur->retain && cur->topicLength == packet->topicLength && memcmp(cur->topic(), packet->topic(), packet->topicLength) == 0) {
                    if (maxSize && (_size - cur->size() + packet->size()) > maxSize) {
                        result = false;
                        break;
                    }
                    packet->next = cur->next;
                    if (prev) {
                        prev->next = packet;
                    }
                    else {
                        _head = packet;
                    }
                    if (_tail == cur) {
                        _tail = packet;
                    }
                    _size = _size - cur->size() + packet->size();
                    replaced = cur;
                    break;
                }
            }
        }
        if (result && !replaced) {
            if (maxSize && (_size + packet->size()) > maxSize) {
                result = false;
            }
            else {
                packet->next = nullptr;
                if (_tail) {
                    _tail->next = packet;
                }
                else {
                    _head = packet;
                }
                _tail = packet;
                _size += packet->size();
                _count++;
            }
        }
        PACKET_LIST_UNLOCK();
        if (replaced) {
            free(replaced);
        }
        if (!result) {
            free(packet);
        }
        return result;
    }

    Packet *PacketList::pop()
    {
        PACKET_LIST_LOCK();
        auto packet = _head;
        if (packet) {
            _head = packet->next;
            if (!_head) {
                _tail = nullptr;
            }
            _size -= packet->size();
            _count--;
        }
        PACKET_LIST_UNLOCK();
        return packet;
    }

    void PacketList::clear()
    {
        PACKET_LIST_LOCK();
        auto packet = _head;
        _head = nullptr;
        _tail = nullptr;
        _size = 0;
        _count = 0;
        PACKET_LIST_UNLOCK();
        while (packet) {
            auto next = packet->next;
            free(packet);
            packet = next;
        }
    }

    // ------------------------------------------------------------------------
    // Client
    // ------------------------------------------------------------------------

    Client *Client::_client;
    ComponentVector Client::_components;
    PacketList Client::_inbox;
    Packet *Client::_rxPacket;
    String Client::_rxDiscoveryPrefix;
    uint16_t Client::_rxDropped;
    uint8_t Client::_generation;

    void Client::setupInstance()
    {
        if (_client || !Plugins::MqttClient::isEnabled()) {
            return;
        }
        _rxDiscoveryPrefix = Plugins::MqttClient::getAutoDiscoveryPrefix();
        _rxDiscoveryPrefix += '/';
        new Client(); // sets _client
        LOOP_FUNCTION_ADD(_loopFunction);
    }

    void Client::deleteInstance()
    {
        if (!_client) {
            return;
        }
        LoopFunctions::remove(_loopFunction);
        delete _client;
        _client = nullptr;
        _inbox.clear();
        if (_rxPacket) {
            free(_rxPacket);
            _rxPacket = nullptr;
        }
    }

    Client::Client() :
        _config(Plugins::MqttClient::getConfig()),
        _hostname(Plugins::MqttClient::getHostname()),
        _username(Plugins::MqttClient::getUsername()),
        _password(Plugins::MqttClient::getPassword()),
        _lastWillTopic(formatTopic(MQTT_LAST_WILL_TOPIC)),
        _lastWillPayload(FPSTR(MQTT_LAST_WILL_TOPIC_OFFLINE)),
        _port(_config.getPort()),
        _current(nullptr),
        _deadline(0),
        _reconnectDelay(_config.auto_reconnect_min),
        _ackTime(0),
        _retryTime(0),
        _connectedTime(0),
        _ackId(0),
        _retries(0),
        _dropped(0),
        _state(StateType::IDLE),
        _resolved(false),
        _reconnect(true)
    {
        // the zeroconf callback can be called before the constructor returns
        _client = this;

        // kfc-<unique id>, the library has the buffer
        size_t maxLength;
        auto clientId = _mqtt.getClientId(maxLength);
        PrintString id(F("kfc-"));
        WebTemplate::printUniqueId(id, FSPGM(kfcfw));
        strlcpy(clientId, id.c_str(), maxLength);
        _mqtt.setClientId(clientId);
        _mqtt.setCredentials(_username.c_str(), _password.c_str());
        _mqtt.setKeepAlive(_config.keepalive);
        _mqtt.setCleanSession(true);
        _mqtt.setWill(_lastWillTopic.c_str(), _getQos(QosType::ONLINE_STATUS), true, _lastWillPayload.c_str(), _lastWillPayload.length());

        // the callbacks run in the context of the TCP stack, they only add events to the inbox
        _mqtt.onConnect([](bool sessionPresent) {
            _pushEvent(Packet::Type::CONNECTED);
        });
        _mqtt.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
            _pushEvent(Packet::Type::DISCONNECTED, static_cast<uint16_t>(reason));
        });
        _mqtt.onPublish([](uint16_t packetId) {
            _pushEvent(Packet::Type::ACK, packetId);
        });
        _mqtt.onSubscribe([](uint16_t packetId, uint8_t qos) {
            _pushEvent(Packet::Type::ACK, packetId);
        });
        _mqtt.onUnsubscribe([](uint16_t packetId) {
            _pushEvent(Packet::Type::ACK, packetId);
        });
        _mqtt.onMessage(_onMessageCallback);
        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, [](WiFiCallbacks::EventType event, void *payload) {
            _pushEvent(event == WiFiCallbacks::EventType::CONNECTED ? Packet::Type::WIFI_UP : Packet::Type::WIFI_DOWN);
        }, this);

        auto generation = ++_generation;
        if (config.hasZeroConf(_hostname)) {
            config.resolveZeroConf(Plugin::getPlugin().getFriendlyName(), _hostname, _port, [generation](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
                if (_client && _generation == generation) {
                    _client->_setServer(hostname, address, port);
                }
            });
        }
        else {
            _setServer(_hostname, convertToIPAddress(_hostname), _port);
        }

        if (KFCFWConfiguration::getWiFiUp()) {
            _onWiFi(true);
        }
    }

    Client::~Client()
    {
        WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);
        _reconnect = false;
        // a forced disconnect: the server publishes the last will
        _mqtt.disconnect(true);
        _closed(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
    }

    void Client::_setServer(const String &hostname, const IPAddress &address, uint16_t port)
    {
        __LDBG_printf("hostname=%s address=%s port=%u", hostname.c_str(), address.toString().c_str(), port);
        _hostname = hostname;
        _address = address;
        _port = port;
        if (IPAddress_isValid(_address)) {
            _mqtt.setServer(_address, _port);
        }
        else {
            _mqtt.setServer(_hostname.c_str(), _port);
        }
        _resolved = true;
    }

    // ------------------------------------------------------------------------
    // components
    // ------------------------------------------------------------------------

    void Client::registerComponent(ComponentPtr component)
    {
        if (component && !isComponentRegistered(component)) {
            _components.push_back(component);
        }
    }

    bool Client::unregisterComponent(ComponentPtr component)
    {
        auto iterator = std::find(_components.begin(), _components.end(), component);
        if (iterator == _components.end()) {
            return false;
        }
        _components.erase(iterator);
        if (_client) {
            auto &subscriptions = _client->_subscriptions;
            for (size_t i = 0; i < subscriptions.size();) {
                if (subscriptions[i].component == component) {
                    String topic = subscriptions[i].topic;
                    _client->unsubscribe(component, topic); // removes the entry
                }
                else {
                    i++;
                }
            }
        }
        return true;
    }

    bool Client::isComponentRegistered(ComponentPtr component)
    {
        return std::find(_components.begin(), _components.end(), component) != _components.end();
    }

    // ------------------------------------------------------------------------
    // state machine
    // ------------------------------------------------------------------------

    void Client::_loopFunction()
    {
        if (_client) {
            _client->_loop();
        }
    }

    void Client::_loop()
    {
        _processInbox();
        auto now = millis();
        switch (_state) {
            case StateType::WAIT:
                if (_resolved && timeReached(now, _deadline)) {
                    _connect(now);
                }
                break;
            case StateType::CONNECTING:
                if (timeReached(now, _deadline)) {
                    _abort(F("Connection to MQTT server %s timed out"));
                }
                break;
            case StateType::CONNECTED:
                _pump(now);
                break;
            case StateType::IDLE:
                break;
        }
    }

    void Client::_processInbox()
    {
        Packet *packet;
        while ((packet = _inbox.pop()) != nullptr) {
            switch (packet->type) {
                case Packet::Type::CONNECTED:
                    _onConnected();
                    break;
                case Packet::Type::DISCONNECTED:
                    _onDisconnected(static_cast<AsyncMqttClientDisconnectReason>(packet->value));
                    break;
                case Packet::Type::ACK:
                    if (_ackId && packet->value == _ackId) {
                        _ackId = 0;
                    }
                    break;
                case Packet::Type::WIFI_UP:
                    _onWiFi(true);
                    break;
                case Packet::Type::WIFI_DOWN:
                    _onWiFi(false);
                    break;
                case Packet::Type::MESSAGE:
                case Packet::Type::DISCOVERY_TOPIC:
                    if (isConnected()) {
                        _onMessage(packet);
                    }
                    break;
                default:
                    break;
            }
            free(packet);
        }
    }

    void Client::_onWiFi(bool up)
    {
        __LDBG_printf("wifi=%u state=%s", up, _getStateString(_state));
        if (up) {
            if (_state == StateType::IDLE && _reconnect) {
                // devices that get WiFi at the same time (access point restarted) do not connect at the same time
                _reconnectDelay = _config.auto_reconnect_min;
                _state = StateType::WAIT;
                _deadline = millis() + (getRandom() % 3000);
            }
        }
        else if (_state != StateType::IDLE) {
            if (_state == StateType::CONNECTED || _state == StateType::CONNECTING) {
                _mqtt.disconnect(true);
                _closed(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
            }
            _state = StateType::IDLE;
        }
    }

    void Client::_connect(uint32_t now)
    {
        if (!KFCFWConfiguration::getWiFiUp()) {
            // ESPAsyncTCP leaks its tcp_pcb when tcp_connect() fails without a route. the WiFi event connects again
            _state = StateType::IDLE;
            return;
        }
        __LDBG_printf("connecting to %s:%u", _hostname.c_str(), _port);
        _state = StateType::CONNECTING;
        _deadline = now + (kConnectTimeout + _config.keepalive) * 1000U;
        _mqtt.connect();
    }

    void Client::_onConnected()
    {
        if (_state != StateType::CONNECTING) {
            // the connection has been given up already
            _mqtt.disconnect(true);
            return;
        }
        _state = StateType::CONNECTED;
        _connectedTime = millis();
        _reconnectDelay = _config.auto_reconnect_min;
        _ackId = 0;

        PrintString server;
        _printServer(server);
        Logger_notice(F("Connected to MQTT server %s"), server.c_str());

        #if MQTT_SET_LAST_WILL_MODE == 2
            publish(formatTopic(MQTT_AVAILABILITY_TOPIC), true, FPSTR(MQTT_AVAILABILITY_TOPIC_ONLINE), QosType::ONLINE_STATUS);
        #endif
        publish(_lastWillTopic, true, FPSTR(MQTT_LAST_WILL_TOPIC_ONLINE), QosType::ONLINE_STATUS);
        // the last will of the previous connection can arrive after "online", it is answered with "online" again
        _enqueue(Packet::Type::SUBSCRIBE, _lastWillTopic, nullptr, 0, QosType::ONLINE_STATUS, false);

        for (size_t i = 0; i < _components.size(); i++) {
            _components[i]->onConnect();
        }

        #if MQTT_AUTO_DISCOVERY && MQTT_AUTO_DISCOVERY_ON_CONNECT
            if (_config.auto_discovery) {
                // devices that connect at the same time (server or access point restarted) spread their auto discovery
                _startDiscovery(getRandom() % (_config.auto_discovery_delay * 1000U + 1), nullptr);
            }
        #endif
    }

    void Client::_onDisconnected(AsyncMqttClientDisconnectReason reason)
    {
        if (_state != StateType::CONNECTED && _state != StateType::CONNECTING) {
            // the connection has been closed already
            return;
        }
        if (_state == StateType::CONNECTED) {
            Logger_notice(F("Disconnected from MQTT server, reason: %s"), _getReasonString(reason));
        }
        _closed(reason);
        _scheduleReconnect();
    }

    void Client::_closed(AsyncMqttClientDisconnectReason reason)
    {
        auto wasConnected = (_state == StateType::CONNECTED);
        _state = StateType::IDLE;
        _discovery.reset();
        _outbox.clear();
        if (_current) {
            free(_current);
            _current = nullptr;
        }
        _subscriptions.clear();
        _ackId = 0;
        if (wasConnected) {
            for (size_t i = 0; i < _components.size(); i++) {
                _components[i]->onDisconnect(reason);
            }
        }
    }

    void Client::_scheduleReconnect()
    {
        if (!_reconnect || !KFCFWConfiguration::getWiFiUp()) {
            _state = StateType::IDLE;
            return;
        }
        // +-25% so that devices that lost the connection at the same time do not reconnect at the same time
        uint32_t delay = (_reconnectDelay * (75 + (getRandom() % 51))) / 100U;
        _state = StateType::WAIT;
        _deadline = millis() + delay;
        PrintString server;
        _printServer(server);
        Logger_notice(F("Reconnecting to MQTT server %s in %.1f seconds"), server.c_str(), delay / 1000.0);
        _reconnectDelay = std::min<uint32_t>(_config.auto_reconnect_max, _reconnectDelay + ((_reconnectDelay * _config.auto_reconnect_incr) / 100U));
    }

    void Client::_abort(const __FlashStringHelper *message)
    {
        PrintString server;
        _printServer(server);
        Logger_notice(message, server.c_str());
        _mqtt.disconnect(true);
        _closed(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
        _scheduleReconnect();
    }

    void Client::connect()
    {
        _reconnect = true;
        _reconnectDelay = _config.auto_reconnect_min;
        if (_state == StateType::IDLE || _state == StateType::WAIT) {
            _state = StateType::WAIT;
            _deadline = millis();
        }
    }

    void Client::disconnect(bool reconnect)
    {
        _reconnect = reconnect;
        if (_state == StateType::CONNECTED) {
            // after a clean disconnect the server does not publish the last will
            #if MQTT_SET_LAST_WILL_MODE == 2
                String availability = formatTopic(MQTT_AVAILABILITY_TOPIC);
                String offline = FPSTR(MQTT_AVAILABILITY_TOPIC_OFFLINE);
                _mqtt.publish(availability.c_str(), _getQos(QosType::ONLINE_STATUS), true, offline.c_str(), offline.length());
            #endif
            _mqtt.publish(_lastWillTopic.c_str(), _getQos(QosType::ONLINE_STATUS), true, _lastWillPayload.c_str(), _lastWillPayload.length());
            _mqtt.disconnect(false);
        }
        else if (_state == StateType::CONNECTING) {
            _mqtt.disconnect(true);
        }
        _closed(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
        if (reconnect) {
            _scheduleReconnect();
        }
    }

    // ------------------------------------------------------------------------
    // outbox
    // ------------------------------------------------------------------------

    void Client::_pump(uint32_t now)
    {
        if (_ackId) {
            if (timeReached(now, _ackTime + kAckTimeout)) {
                _abort(F("MQTT server %s did not acknowledge a packet"));
            }
            return;
        }
        if (!_current) {
            _current = _outbox.pop();
            if (!_current) {
                // nothing else to send
                if (_discovery) {
                    _discovery->loop(now);
                    if (_discovery && _discovery->isDone()) {
                        _discovery.reset();
                    }
                }
                return;
            }
            _retries = 0;
        }
        if (!timeReached(now, _retryTime)) {
            return;
        }
        // the packet is handed to the library only if it fits into the TCP buffer. the library never holds more
        // than one message
        if (_mqtt.getAsyncClient().space() < (_current->topicLength + _current->payloadLength + 16U)) {
            return;
        }
        uint16_t packetId = 0;
        switch (_current->type) {
            case Packet::Type::PUBLISH:
                packetId = _mqtt.publish(_current->topic(), _current->qos, _current->retain, _current->payload(), _current->payloadLength);
                break;
            case Packet::Type::SUBSCRIBE:
                packetId = _mqtt.subscribe(_current->topic(), _current->qos);
                break;
            case Packet::Type::UNSUBSCRIBE:
                packetId = _mqtt.unsubscribe(_current->topic());
                break;
            default:
                break;
        }
        if (!packetId) {
            // the library refuses to publish if the largest free block is below MQTT_MIN_FREE_MEMORY
            if (++_retries >= kMaxRetries) {
                Logger_error(F("MQTT message dropped, not enough memory: %s"), _current->topic());
                free(_current);
                _current = nullptr;
                _dropped++;
            }
            else {
                _retryTime = now + 100;
            }
            return;
        }
        if (_discovery) {
            // the library has a copy of the message, ours is still allocated
            _discovery->sample();
        }
        if (_current->type != Packet::Type::PUBLISH || _current->qos) {
            _ackId = packetId;
            _ackTime = now;
        }
        free(_current);
        _current = nullptr;
    }

    bool Client::_enqueue(Packet::Type type, const String &topic, const char *payload, size_t payloadLength, QosType qos, bool retain)
    {
        if (topic.length() + payloadLength > MQTT_MAX_MESSAGE_SIZE) {
            Logger_error(F("MQTT maximum message size exceeded: %u/%u: %s"), topic.length() + payloadLength, MQTT_MAX_MESSAGE_SIZE, topic.c_str());
            return false;
        }
        auto packet = Packet::create(type, topic.c_str(), topic.length(), payload, payloadLength, _getQos(qos), retain);
        // push() deletes the packet if the outbox is full
        if (!packet || !_outbox.push(packet, MQTT_OUTBOX_MAX_SIZE, type == Packet::Type::PUBLISH && retain)) {
            _dropped++;
            return false;
        }
        return true;
    }

    bool Client::subscribe(ComponentPtr component, const String &topic, QosType qos)
    {
        if (!isConnected() || !topic.length()) {
            return false;
        }
        bool subscribed = false;
        for (const auto &subscription : _subscriptions) {
            if (subscription.topic == topic) {
                if (subscription.component == component) {
                    return true;
                }
                subscribed = true;
            }
        }
        if (!subscribed && !_enqueue(Packet::Type::SUBSCRIBE, topic, nullptr, 0, qos, false)) {
            return false;
        }
        _subscriptions.emplace_back(topic, component);
        return true;
    }

    bool Client::unsubscribe(ComponentPtr component, const String &topic)
    {
        auto size = _subscriptions.size();
        _subscriptions.erase(std::remove_if(_subscriptions.begin(), _subscriptions.end(), [component, &topic](const Subscription &subscription) {
            return subscription.component == component && subscription.topic == topic;
        }), _subscriptions.end());
        if (size == _subscriptions.size()) {
            return false;
        }
        if (!isConnected()) {
            return true;
        }
        for (const auto &subscription : _subscriptions) {
            if (subscription.topic == topic) {
                // another component uses the topic
                return true;
            }
        }
        return _enqueue(Packet::Type::UNSUBSCRIBE, topic, nullptr, 0, QosType::AT_LEAST_ONCE, false);
    }

    bool Client::publish(const String &topic, bool retain, const String &payload, QosType qos)
    {
        if (!isConnected() || !topic.length()) {
            return false;
        }
        return _enqueue(Packet::Type::PUBLISH, topic, payload.c_str(), payload.length(), qos, retain);
    }

    uint8_t Client::_getQos(QosType qos) const
    {
        if (qos == QosType::DEFAULT) {
            qos = static_cast<QosType>(_config.qos);
        }
        return std::min<uint8_t>(static_cast<uint8_t>(qos), 2);
    }

    // ------------------------------------------------------------------------
    // incoming messages
    // ------------------------------------------------------------------------

    void Client::_pushEvent(Packet::Type type, uint16_t value)
    {
        auto packet = Packet::create(type, nullptr, 0, nullptr, 0, 0, false, value);
        if (packet) {
            _inbox.push(packet);
        }
    }

    void Client::_onMessageCallback(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
    {
        if (index == 0) {
            if (_rxPacket) {
                // the previous message is incomplete
                free(_rxPacket);
                _rxPacket = nullptr;
            }
            auto topicLength = strlen(topic);
            // the retained auto discovery of this device: only the topic is needed
            if (properties.retain && topicLength > 7 && strncmp(topic, _rxDiscoveryPrefix.c_str(), _rxDiscoveryPrefix.length()) == 0 && strcmp_P(topic + topicLength - 7, PSTR("/config")) == 0) {
                auto packet = Packet::create(Packet::Type::DISCOVERY_TOPIC, topic, topicLength);
                if (!packet || !_inbox.push(packet, kInboxMaxSize)) {
                    _rxDropped++;
                }
                return;
            }
            if (total > MQTT_RECV_MAX_MESSAGE_SIZE) {
                _rxDropped++;
                return;
            }
            _rxPacket = Packet::create(Packet::Type::MESSAGE, topic, topicLength, nullptr, total, properties.qos, properties.retain);
            if (!_rxPacket) {
                _rxDropped++;
                return;
            }
        }
        if (!_rxPacket || (index + len) > _rxPacket->payloadLength) {
            return;
        }
        memcpy(_rxPacket->payload() + index, payload, len);
        if ((index + len) == total) {
            auto packet = _rxPacket;
            _rxPacket = nullptr;
            if (!_inbox.push(packet, kInboxMaxSize)) {
                _rxDropped++;
            }
        }
    }

    void Client::_onMessage(Packet *packet)
    {
        auto topic = packet->topic();
        if (packet->type == Packet::Type::DISCOVERY_TOPIC) {
            if (_discovery) {
                _discovery->onTopic(topic);
            }
            return;
        }
        if (_lastWillTopic == topic) {
            // another client or the last will of the previous connection marked the device as offline
            if (strcmp_P(packet->payload(), MQTT_LAST_WILL_TOPIC_ONLINE) != 0) {
                publish(_lastWillTopic, true, FPSTR(MQTT_LAST_WILL_TOPIC_ONLINE), QosType::ONLINE_STATUS);
            }
            return;
        }
        // a component can subscribe or unsubscribe inside onMessage()
        for (size_t i = 0; i < _subscriptions.size(); i++) {
            if (isTopicMatch(topic, _subscriptions[i].topic.c_str())) {
                _subscriptions[i].component->onMessage(topic, packet->payload(), packet->payloadLength);
            }
        }
    }

    // ------------------------------------------------------------------------
    // auto discovery
    // ------------------------------------------------------------------------

    bool Client::publishAutoDiscovery(AutoDiscovery::StatusCallback callback)
    {
        #if MQTT_AUTO_DISCOVERY
            if (!isConnected() || !_config.auto_discovery) {
                return false;
            }
            _startDiscovery(MQTT_DISCOVERY_REQUEST_DELAY_MS, callback);
            return true;
        #else
            return false;
        #endif
    }

    bool Client::isAutoDiscoveryRunning() const
    {
        return _discovery != nullptr;
    }

    void Client::_startDiscovery(uint32_t delayMillis, AutoDiscovery::StatusCallback callback)
    {
        // a running auto discovery reports a failure and unsubscribes
        _discovery.reset();
        _discovery.reset(new AutoDiscovery::Discovery(*this, delayMillis, callback));
    }

    // ------------------------------------------------------------------------
    // topics
    // ------------------------------------------------------------------------

    String Client::formatTopic(const String &componentName, const __FlashStringHelper *format, ...)
    {
        PrintString topic = getBaseTopic();
        if (componentName.length()) {
            if (componentName.charAt(0) != '/') {
                topic += '/';
            }
            topic.print(componentName);
        }
        if (format) {
            if (!componentName.length() && pgm_read_byte(RFPSTR(format)) != '/') {
                topic += '/';
            }
            va_list arg;
            va_start(arg, format);
            topic.vprintf_P(RFPSTR(format), arg);
            va_end(arg);
        }
        return topic;
    }

    String Client::formatTopic(const __FlashStringHelper *format, ...)
    {
        PrintString topic = getBaseTopic();
        if (format) {
            if (pgm_read_byte(RFPSTR(format)) != '/') {
                topic += '/';
            }
            va_list arg;
            va_start(arg, format);
            topic.vprintf_P(RFPSTR(format), arg);
            va_end(arg);
        }
        return topic;
    }

    String Client::getBaseTopic()
    {
        String topic = Plugins::MqttClient::getBaseTopic();
        if (topic.indexOf(F("${device_name}")) != -1) {
            topic.replace(F("${device_name}"), filterString(System::Device::getName()));
        }
        if (topic.indexOf(F("${object_id}")) != -1) {
            topic.replace(F("${object_id}"), filterString(System::Device::getObjectIdOrName(), true));
        }
        if (topic.indexOf(F("${device_title}")) != -1) {
            topic.replace(F("${device_title}"), filterString(System::Device::getTitle()));
        }
        if (topic.indexOf(F("${device_title_no_space}")) != -1) {
            topic.replace(F("${device_title_no_space}"), filterString(System::Device::getTitle(), true));
        }
        StrWrapper(topic).rtrim('/');
        return topic;
    }

    String Client::getBaseTopicPrefix()
    {
        String baseTopic = filterString(System::Device::getObjectIdOrName(), true);
        if (!StrView(baseTopic).endsWith('_')) {
            baseTopic += '_';
        }
        return baseTopic;
    }

    String Client::filterString(const char *str, bool replaceSpace)
    {
        String out = FPSTR(str);
        for (size_t i = 0; i < out.length(); i++) {
            auto ch = out.charAt(i);
            if (replaceSpace) {
                if (ch == ' ') {
                    out.setCharAt(i, '_');
                }
                else {
                    out.setCharAt(i, tolower(ch));
                }
            }
            else if (!isalnum(ch) || !isprint(ch)) {
                out.remove(i--, 1);
            }
        }
        return out;
    }

    bool Client::isTopicMatch(const char *topic, const char *match)
    {
        while (*topic) {
            switch (*match) {
                case 0:
                    return false;
                case '#':
                    return true;
                case '+':
                    match++;
                    while (*topic && *topic != '/') {
                        topic++;
                    }
                    if (!*topic && !*match) {
                        return true;
                    }
                    else if (*topic != *match) {
                        return false;
                    }
                    break;
                default:
                    if (*topic != *match) {
                        return false;
                    }
                    break;
            }
            topic++;
            match++;
        }
        // "a/#" matches "a" as well
        return *match == 0 || *match == '#' || (match[0] == '/' && match[1] == '#' && match[2] == 0);
    }

    int8_t Client::toBool(const char *str, int8_t invalid)
    {
        while (isspace(*str)) {
            str++;
        }
        if (!*str) {
            return invalid;
        }
        // an integer
        char *end = nullptr;
        auto value = strtol(str, &end, 0);
        while (isspace(*end)) {
            end++;
        }
        if (end && !*end) {
            return value != 0;
        }
        auto tmp = String(str);
        StrWrapper(tmp).trim();
        auto cTmp = tmp.c_str();
        if ((strcasecmp_P(cTmp, PSTR("on")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("yes")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("enable")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("enabled")) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_bool_on)) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_status_topic_online)) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_bool_true)) == 0)
        ) {
            return true;
        }
        if ((strcasecmp_P(cTmp, PSTR("off")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("no")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("disable")) == 0) ||
            (strcasecmp_P(cTmp, PSTR("disabled")) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_bool_off)) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_status_topic_offline)) == 0) ||
            (strcasecmp_P(cTmp, SPGM(mqtt_bool_false)) == 0)
        ) {
            return false;
        }
        return invalid;
    }

    String Client::toRGB(uint32_t color)
    {
        return PrintString(F("%u,%u,%u"), ((color >> 16) & 0xff), ((color >> 8) & 0xff), (color & 0xff));
    }

    // ------------------------------------------------------------------------
    // status
    // ------------------------------------------------------------------------

    void Client::_printServer(Print &output) const
    {
        if (_username.length()) {
            output.print(_username);
        }
        else {
            output.print(F("Anonymous"));
        }
        output.print('@');
        if (IPAddress_isValid(_address)) {
            _address.printTo(output);
        }
        else {
            output.print(_hostname);
        }
        output.print(':');
        output.print(_port);
    }

    const __FlashStringHelper *Client::_getStateString(StateType state)
    {
        switch (state) {
            case StateType::WAIT:
                return F("waiting to reconnect");
            case StateType::CONNECTING:
                return F("connecting");
            case StateType::CONNECTED:
                return F("connected");
            case StateType::IDLE:
                break;
        }
        return F("disconnected");
    }

    const __FlashStringHelper *Client::_getReasonString(AsyncMqttClientDisconnectReason reason)
    {
        switch (reason) {
            case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
                return F("Network disconnect");
            case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                return F("Unacceptable protocol version");
            case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
                return F("Identifier rejected");
            case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
                return F("Server unavailable");
            case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                return F("Malformed credentials");
            case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
                return F("Not authorized");
            case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
                return F("ESP8266 not enough space");
            case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
                return F("Bad fingerprint");
        }
        return F("Unknown");
    }

    void Client::printStatus(Print &output)
    {
        output.print(_getStateString(_state));
        output.print(' ');
        _printServer(output);
        output.printf_P(PSTR(", QoS %u" HTML_S(br) "Topic %s"), _getQos(QosType::DEFAULT), getBaseTopic().c_str());
        #if MQTT_AUTO_DISCOVERY
            if (_config.auto_discovery) {
                size_t entities = 0;
                for (auto component : _components) {
                    entities += component->getAutoDiscoveryCount();
                }
                output.printf_P(PSTR(HTML_S(br) "Auto discovery %s, %u entities"), Plugins::MqttClient::getAutoDiscoveryPrefix(), entities);
                if (_discovery) {
                    output.print(F(", publishing"));
                }
                else if (_discoveryStats.finished && _discoveryStats.result == StatusType::SUCCESS) {
                    output.printf_P(PSTR(", published %u minutes ago"), (millis() - _discoveryStats.finished) / 60000U);
                }
            }
        #endif
    }

    void Client::printDebug(Print &output)
    {
        auto now = millis();
        output.printf_P(PSTR("state=%s server="), _getStateString(_state));
        _printServer(output);
        output.printf_P(PSTR(" qos=%u keepalive=%u resolved=%u reconnect=%u\n"), _getQos(QosType::DEFAULT), _config.keepalive, _resolved, _reconnect);
        if (_state == StateType::CONNECTED) {
            output.printf_P(PSTR("connected=%us"), (now - _connectedTime) / 1000U);
        }
        else if (_state == StateType::WAIT || _state == StateType::CONNECTING) {
            output.printf_P(PSTR("next=%dms"), static_cast<int32_t>(_deadline - now));
        }
        output.printf_P(PSTR(" reconnect_delay=%ums\n"), _reconnectDelay);
        output.printf_P(PSTR("outbox=%u/%u bytes packets=%u current=%u ack_wait=%u dropped=%u inbox=%u received_dropped=%u subscriptions=%u\n"),
            _outbox.size(), MQTT_OUTBOX_MAX_SIZE, _outbox.count(), _current != nullptr, _ackId ? (now - _ackTime) : 0, _dropped, _inbox.count(), _rxDropped, _subscriptions.size());
        #if MQTT_AUTO_DISCOVERY
            output.printf_P(PSTR("auto discovery=%u prefix=%s start_window=%us phase=%s\n"), _config.auto_discovery, Plugins::MqttClient::getAutoDiscoveryPrefix(),
                _config.auto_discovery_delay, _discovery ? reinterpret_cast<PGM_P>(AutoDiscovery::Discovery::getPhaseName(_discovery->getPhase())) : PSTR("-"));
            auto &stats = _discoveryStats;
            PGM_P result = stats.result == StatusType::SUCCESS ? PSTR("success") : (stats.result == StatusType::FAILURE ? PSTR("failure") : PSTR("running/none"));
            output.printf_P(PSTR("last: result=%s entities=%u removed=%u skipped=%u duration=%ums finished=%ds ago\n"),
                result, stats.entities, stats.removed, stats.skipped, stats.duration, stats.finished ? static_cast<int>((now - stats.finished) / 1000U) : -1);
            const AutoDiscovery::MemoryStats *phases[] = { &stats.collect, &stats.remove, &stats.publish };
            for (uint8_t i = 0; i < 3; i++) {
                auto &mem = *phases[i];
                output.printf_P(PSTR("memory %s: heap=%u min=%u used=%u block=%u min=%u\n"),
                    AutoDiscovery::Discovery::getPhaseName(static_cast<AutoDiscovery::Discovery::PhaseType>(i + 1)),
                    mem.heapStart, mem.heapMin, mem.heapStart - mem.heapMin, mem.blockStart, mem.blockMin);
            }
        #endif
    }

}
