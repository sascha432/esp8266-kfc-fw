/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <EventScheduler.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "mqtt_def.h"
#include "mqtt_plugin.h"
#include "mqtt_strings.h"
#include "auto_discovery.h"
#include "logger.h"
#include "templates.h"

#if DEBUG_MQTT_CLIENT
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;
using MqttClient = KFCConfigurationClasses::Plugins::MQTTConfigNS::MqttClient;

namespace MQTT {

    ComponentVector Client::_components;
    Client *Client::_mqttClient;

    PacketQueue::PacketQueue(uint16_t packetId, uint32_t time, bool delivered, uint16_t internalPacketId) :
        _packetId(packetId),
        _internalId(internalPacketId == 0 ? MQTT::Client::getNextInternalPacketId() : internalPacketId),
        _delivered(delivered),
        _time(time),
        _timeout(kDefaultAckTimeout)
    {
    }

    bool PacketQueueVector::setTimeout(uint16_t internalId, uint16_t timeout)
    {
        auto queue = find(PacketQueueInternalId(internalId));
        if (queue) {
            queue->setTimeout(timeout);
            return true;
        }
        return false;
    }

    void Client::setupInstance()
    {
        __LDBG_printf("enabled=%u", System::Flags::getConfig().is_mqtt_enabled);
        #if DEBUG
            if (_mqttClient) {
                __LDBG_panic("MQTT client already exists");
            }
        #endif
        if (System::Flags::getConfig().is_mqtt_enabled) {
            _mqttClient = new Client();
        }
    }

    void Client::deleteInstance()
    {
        __LDBG_printf("client=%p", _mqttClient);
        if (_mqttClient) {
            delete _mqttClient;
            _mqttClient = nullptr;
        }
    }

    #include <HeapStream.h>

    Client::Client() :
        _hostname(Plugins::MqttClient::getHostname()),
        _username(Plugins::MqttClient::getUsername()),
        _password(Plugins::MqttClient::getPassword()),
        _config(Plugins::MqttClient::getConfig()),
        _client(new AsyncMqttClient()),
        _port(_config.getPort()),
        _lastWillTopic(formatTopic(MQTT_LAST_WILL_TOPIC)),
        _connState(ConnectionState::NONE),
        #if MQTT_AUTO_DISCOVERY
            #if IOT_REMOTE_CONTROL
                _startAutoDiscovery(false),
            #else
                _startAutoDiscovery(true),
            #endif
        #endif
        _messageProperties(nullptr)
    {
        size_t maxLength = 1;
        char *clientIdBuffer = _client->getClientId(maxLength);
        PrintString buffer = F("kfc-");
        WebTemplate::printUniqueId(buffer, FSPGM(kfcfw));
        strncpy(clientIdBuffer, buffer.c_str(), maxLength - 1)[maxLength - 1] = 0;
        _client->setClientId(clientIdBuffer);

        // add client to all components and call onStartup
        for(auto component: _components) {
            component->setClient(this);
            component->onStartup();
        }

        // connect to server
        __LDBG_printf("hostname=%s port=%u username=%s client_id=%s", _hostname.c_str(), _port, _username.c_str(), __S(_client->getClientId()));
        if (config.hasZeroConf(_hostname)) {
            config.resolveZeroConf(Plugin::getPlugin().getFriendlyName(), _hostname, _port, [this](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
                this->_zeroConfCallback(hostname, address, port, type);
            });
        }
        else {
            _zeroConfCallback(_hostname, convertToIPAddress(_hostname), _port, MDNSResolver::ResponseType::NONE);
        }

    }

    Client::~Client()
    {
        __LDBG_printf("conn=%s", _connection());
        WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, MQTT::Client::handleWiFiEvents);
        _resetClient();
        _Timer(_reconnectTimer).remove();

        _autoReconnectTimeout = kAutoReconnectDisabled;
        auto state = setConnState(ConnectionState::DISCONNECTED);

        // call onDisconnect() and remove client from all components to avoid being called without a client
        for(auto component: _components) {
            if (state == ConnectionState::CONNECTED) {
                component->onDisconnect(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
            }
            component->onShutdown();
            component->setClient(nullptr);
        }
        disconnect(true);
        delete _client;
    }

    void Client::_resetClient()
    {
        __LDBG_printf("queue=%p rebroadcast_timer=%u queue_timer=%u queue_size=%u conn=%s", _autoDiscoveryQueue.get(), (bool)_autoDiscoveryRebroadcast, (bool)_queue.getTimer(), _queue.size(), _connection());
        MUTEX_LOCK_RECURSIVE_BLOCK(_lock) {
            _autoDiscoveryQueue.reset();
            _autoDiscoveryRebroadcast.remove();
            _queue.clear();
            _packetQueue.clear();
        }
    }

    void Client::_zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
    {
        _address = address;
        _hostname = hostname;
        _port = port;
        __LDBG_printf("zeroconf address=%s hostname=%s port=%u type=%u", _address.toString().c_str(), _hostname.c_str(), _port, type);

        _setupClient();
        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, MQTT::Client::handleWiFiEvents);

        if (WiFi.isConnected()) {
            connect();
        }
    }

    void Client::_setupClient()
    {
        __LDBG_printf("hostname=%s port=%u", _hostname.c_str(), _port);
        _resetClient();

        _autoReconnectTimeout = _config.auto_reconnect_min;

        if (IPAddress_isValid(_address)) {
            _client->setServer(_address, _port);
        }
        else {
            _client->setServer(_hostname.c_str(), _port);
        }
        #if ASYNC_TCP_SSL_ENABLED
            if (_config._get_enum_mode() == ModeType::SECURE) {
                _client->setSecure(true);
                uint16_t size = MqttClient::kFingerprintMaxSize;
                auto fingerPrint = MqttClient::getFingerPrint(size);
                if (fingerPrint && size == MqttClient::kFingerprintMaxSize) {
                    _client->addServerFingerprint(fingerPrint); // addServerFingerprint supports multiple fingerprints
                }
            }
        #endif
        _client->setCredentials(_username.c_str(), _password.c_str());
        _client->setKeepAlive(_config.keepalive);

        _client->onConnect([](bool sessionPresent) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->onConnect(sessionPresent);
            }
        });
        _client->onDisconnect([](AsyncMqttClientDisconnectReason reason) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->onDisconnect(reason);;
            }
        });
        _client->onMessage([](char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->onMessageRaw(topic, payload, properties, len, index, total);
            }
        });
        _client->onPublish([](uint16_t packetId) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->_onPacketAck(packetId, PacketAckType::PUBLISH);
            }
        });
        _client->onSubscribe([](uint16_t packetId, uint8_t qos) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->_onPacketAck(packetId, PacketAckType::SUBSCRIBE);
            }
        });
        _client->onUnsubscribe([](uint16_t packetId) {
            MQTT::Client *client;
            if ((client = MQTT::Client::getClient()) != nullptr) {
                client->_onPacketAck(packetId, PacketAckType::UNSUBSCRIBE);
            }
        });
    }

    //
    // Components
    //

    void Client::registerComponent(ComponentPtr component)
    {
        if (_mqttClient) {
            component->setClient(_mqttClient);
            _mqttClient->_registerComponent(component);
        }
        else {
            if (!isComponentRegistered(component)) {
                _components.push_back(component);
            }
        }
    }

    bool Client::unregisterComponent(ComponentPtr component)
    {
        auto size = _components.size();
        if (_mqttClient) {
            auto result = _mqttClient->_unregisterComponent(component);
            component->setClient(nullptr);
            return result;
        }
        else {
            _components.erase(std::remove(_components.begin(), _components.end(), component), _components.end());
        }
        return size != _components.size();
    }

    bool Client::isComponentRegistered(ComponentPtr component)
    {
        return std::find(_components.begin(), _components.end(), component) != _components.end();
    }

    void Client::_registerComponent(ComponentPtr component)
    {
        if (!component) {
            __DBG_assertf(false, "registerComponent(nullptr)");
            return;
        }
        __LDBG_printf("component=%p type=%s", component, component->getName());
        MUTEX_LOCK_RECURSIVE_BLOCK(_lock) {
        if (isComponentRegistered(component)) {
                __DBG_assertf(false, "component=%p type=%s already registered", component, component->getName());
                return;
            }
            // turn off interrupts to avoid issues with running auto discovery
            _components.push_back(component);
        }
        __LDBG_printf("components=%u entities=%u", _components.size(), AutoDiscovery::List::size(_components));
    }

    bool Client::_unregisterComponent(ComponentPtr component)
    {
        if (!component) {
            __DBG_assertf(false, "unregisterComponent(nullptr)");
            return false;
        }
        MUTEX_LOCK_RECURSIVE_BLOCK(_lock) {
            auto size = _components.size();
            if (size) {
                remove(component);
                _components.erase(std::remove(_components.begin(), _components.end(), component), _components.end());
            }
            __LDBG_printf("components=%u entities=%u removed=%u", _components.size(), AutoDiscovery::List::size(_components), _components.size() != size);
            return _components.size() != size;
        }
        __builtin_unreachable();
    }


    void Client::remove(ComponentPtr component)
    {
        __LDBG_printf("component=%p topics=%u", component, _topics.size());
        _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [this, component](const ClientTopic &topic) {
            if (topic.getComponent() == component) {
                __LDBG_printf("topic=%s in_use=%d", topic.getTopic().c_str(), _topicInUse(component, topic));
                if (!_topicInUse(component, topic)) {
                    unsubscribe(nullptr, topic.getTopic());
                }
                return true;
            }
            return false;
        }), _topics.end());
        _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [this, component](const ClientQueue &queue) {
            return (queue.getComponent() == component);
        }), _queue.end());
    }

    //
    // Topics
    //

    String MQTT::Client::formatTopic(const String &componentName, const __FlashStringHelper *format, ...)
    {
        String suffix;
        va_list arg;
        va_start(arg, format);
        String topic = _formatTopic(componentName, format, arg);
        va_end(arg);
        return topic;
    }

    String MQTT::Client::formatTopic(const __FlashStringHelper *format, ...)
    {
        va_list arg;
        va_start(arg, format);
        String topic = _formatTopic(String(), format, arg);
        va_end(arg);
        return topic;
    }

    String MQTT::Client::_filterString(PGM_P str, bool replaceSpace)
    {
        String out = FPSTR(str);
        for(size_t i = 0; i < out.length(); i++) {
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

    bool MQTT::Client::_isTopicMatch(const char *topic, const char *match) const
    {
        while(*topic) {
            switch(*match) {
                case 0:
                    return false;
                case '#':
                    return true;
                case '+':
                    match++;
                    while(*topic && *topic != '/') {
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
        return true;
    }

    String MQTT::Client::_getBaseTopic()
    {
        String topic = Plugins::MqttClient::getBaseTopic();
        // __LDBG_printf("base_topic=%s ${device_name}=%d ${object_id}=%d ${device_title}=%d ${device_title_no_space}=%d",
        //     __S(topic),
        //     topic.indexOf(F("${device_name}")),
        //     topic.indexOf(F("${object_id}")),
        //     topic.indexOf(F("${device_title}")),
        //     topic.indexOf(F("${device_title_no_space}"))
        // );
        if (topic.indexOf(F("${device_name}")) != -1) {
            topic.replace(F("${device_name}"), _filterString(System::Device::getName()));
        }
        if (topic.indexOf(F("${object_id}")) != -1) {
            topic.replace(F("${object_id}"), _filterString(System::Device::getObjectIdOrName(), true));
        }
        if (topic.indexOf(F("${device_title}")) != -1) {
            topic.replace(F("${device_title}"), _filterString(System::Device::getTitle()));
        }
        if (topic.indexOf(F("${device_title_no_space}")) != -1) {
            topic.replace(F("${device_title_no_space}"), _filterString(System::Device::getTitle(), true));
        }
        #if DEBUG_MQTT_CLIENT && 0
            topic.rtrim('/');
            __DBG_printf("base_topic=%s replaced", __S(topic));
            return topic;
        #else
            return topic.rtrim('/');
        #endif
    }

    #if MQTT_GROUP_TOPIC

        bool MQTT::Client::_getGroupTopic(ComponentPtr component, String groupTopic, String &topic)
        {
            String deviceTopic = MQTT::Client::_getBaseTopic();
            if (!topic.startsWith(deviceTopic) || topic.startsWith(groupTopic)) {
                return false;
            }
            if (topic.indexOf(F("${component_name}")) != -1) {
                topic.replace(F("${component_name}"), component->getName());
            }
            groupTopic += &topic.c_str()[deviceTopic.length() - 1];
            topic = std::move(groupTopic);
            String_rtrim(topic, '/');
            return true;
        }

    #endif

    String MQTT::Client::_formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg)
    {
        PrintString topic = MQTT::Client::_getBaseTopic(); // any trailing slashes are stripped
        if (suffix.length()) {
            if (suffix.charAt(0) != '/') { // add slash if missing
                topic += '/';
            }
            topic.print(suffix);
        }
        if (format) {
            if (!suffix.length() && pgm_read_byte(RFPSTR(format)) != '/') { // add slash if missing
                topic += '/';
            }
            topic.vprintf_P(RFPSTR(format), arg);
        }
        __LDBG_assertf(topic.indexOf(F("//")) == -1, "topic '%s' contains //", topic.c_str());
        // __LDBG_printf("topic=%s", topic.c_str());
        return topic;
    }

    #if DEBUG_MQTT_CLIENT

        const __FlashStringHelper *MQTT::Client::_getConnStateStr()
        {
            switch(_connState) {
                case ConnectionState::AUTO_RECONNECT_DELAY:
                    return F("AUTO_RECONNECT_DELAY");
                case ConnectionState::PRE_CONNECT:
                    return F("PRE_CONNECT");
                case ConnectionState::CONNECTING:
                    return F("CONNECTING");
                case ConnectionState::CONNECTED:
                    return F("CONNECTED");
                case ConnectionState::DISCONNECTED:
                    return F("DISCONNECTED");
                case ConnectionState::DISCONNECTING:
                    return F("DISCONNECTING");
                case ConnectionState::IDLE:
                    return F("IDLE");
                case ConnectionState::NONE:
                    break;
            }
            return F("NONE");
        }

        const char *MQTT::Client::_connection()
        {
            DEBUG_HELPER_PUSH_STATE();
            DEBUG_HELPER_SILENT();
            _connectionStr = PrintString(F("%s state=%s auto_reconnect=%u timer=%u"), _client->getAsyncClient().stateToString(), RFPSTR(_getConnStateStr()), _autoReconnectTimeout, (bool)_reconnectTimer);
            DEBUG_HELPER_POP_STATE();
            return _connectionStr.c_str();
        }

    #endif

    void MQTT::Client::connect()
    {
        if ((_connState == ConnectionState::CONNECTING || _connState == ConnectionState::CONNECTED) && _client->connected()) {
            __LDBG_printf("aborting connect, conn=%s", _connection());
            return;
        }
        else {
            __LDBG_printf("conn=%s", _connection());
        }
        _connState = ConnectionState::PRE_CONNECT;

        // remove reconnect timer if running and force disconnect
        auto timeout = _autoReconnectTimeout;
        _Timer(_reconnectTimer).remove();

        _autoReconnectTimeout = kAutoReconnectDisabled; // disable auto reconnect
        disconnect(true);
        _autoReconnectTimeout = timeout; // restore timeout

        _client->setCleanSession(true);
        _client->setKeepAlive(_config.keepalive);

        #if MQTT_SET_LAST_WILL_MODE != 0
            __LDBG_printf("topic=%s value=%s", _lastWillTopic.c_str(), MQTT_LAST_WILL_TOPIC_OFFLINE);
            _client->setWill(_lastWillTopic.c_str(), _translateQosType(getDefaultQos()), true, MQTT_LAST_WILL_TOPIC_OFFLINE);
        #endif

        _connState = ConnectionState::CONNECTING;
        _client->connect();
    }

    void MQTT::Client::disconnect(bool forceDisconnect)
    {
        __LDBG_printf("force_disconnect=%d conn=%s", forceDisconnect, _connection());
        if (setConnState(ConnectionState::DISCONNECTING) == ConnectionState::CONNECTED && !forceDisconnect && _client->connected()) {
            // set offline if last will is not used
            // for example for clients that go to deep sleep
            // when reconfiguring or stopping mqtt, the device is marked offline manually
            __LDBG_printf("marking device as offline manually conn=%s", _connection());
            // clear client to send going offline status
            _resetClient();

            // set last will topic
            _client->publish(_lastWillTopic.c_str(), _translateQosType(getDefaultQos()), true, MQTT_LAST_WILL_TOPIC_OFFLINE);
        }
        _client->disconnect(forceDisconnect);
    }

    void MQTT::Client::autoReconnect(AutoReconnectType timeout)
    {
        __LDBG_printf("timeout=%u clamped=%u conn=%s", timeout, std::clamp<AutoReconnectType>(timeout, _config.auto_reconnect_min, _config.auto_reconnect_max), _connection());
        if (timeout) {
            _connState = ConnectionState::AUTO_RECONNECT_DELAY;
            timeout = std::clamp<AutoReconnectType>(timeout, _config.auto_reconnect_min, _config.auto_reconnect_max);
            Logger_notice(F("Reconnecting to MQTT server %s in %.1f seconds"), connectionDetailsString(false).c_str(), timeout / 1000.0);
            _Timer(_reconnectTimer).add(Event::milliseconds(timeout), false, [this](Event::CallbackTimerPtr timer) {
                __LDBG_printf("timer lambda: conn=%s", _connection());
                if (!this->isConnected()) {
                    this->connect();
                }
            });
            // increase timeout on each reconnect attempt
            setAutoReconnect(timeout + ((uint32_t(timeout) * _config.auto_reconnect_incr) / 100U));
        }
        else {
            _connState = ConnectionState::IDLE;
            _Timer(_reconnectTimer).remove();
            setAutoReconnect(kAutoReconnectDisabled);
        }
    }

    void MQTT::Client::onConnect(bool sessionPresent)
    {
        __LDBG_printf("session=%d conn=%s", sessionPresent, _connection());
        Logger_notice(F("Connected to MQTT server %s"), connectionDetailsString(false).c_str());
        _connState = ConnectionState::CONNECTED;

        _resetClient();

        // set last will topic
        publish(_lastWillTopic, true, FPSTR(MQTT_LAST_WILL_TOPIC_ONLINE), QosType::ONLINE_STATUS);

        // monitor topic in case the status is set to "offline"
        subscribe(nullptr, _lastWillTopic, QosType::ONLINE_STATUS);

        #if MQTT_SET_LAST_WILL_MODE != 2 && IOT_REMOTE_CONTROL == 1
            #error remote control requires MQTT_SET_LAST_WILL_MODE == 2 for MQTT_AVAILABILITY_TOPIC
        #endif

        #if MQTT_AUTO_DISCOVERY
            _autoDiscoveryStatusTopic = _getAutoDiscoveryStatusTopic();
            __LDBG_printf("subscribe=%s", __S(_autoDiscoveryStatusTopic));
            subscribe(nullptr, _autoDiscoveryStatusTopic, QosType::AUTO_DISCOVERY);
        #endif

        // reset reconnect timer if connection was successful
        setAutoReconnect(_config.auto_reconnect_min);

        for(const auto &component: _components) {
            component->onConnect();
        }
        #if MQTT_AUTO_DISCOVERY
            if (_startAutoDiscovery) {
                publishAutoDiscovery();
            }
        #endif
    }

    void MQTT::Client::onDisconnect(AsyncMqttClientDisconnectReason reason)
    {
        __LDBG_printf("reason=%d auto_reconnect=%d conn=%s", (int)reason, _autoReconnectTimeout, _connection());

        // onDisconnect() sets _startAutoDiscovery
        // _startAutoDiscovery = _startAutoDiscovery || isAutoDiscoveryRunning();

        // check if the connection was established
        if (setConnState(ConnectionState::DISCONNECTED) == ConnectionState::CONNECTED) {
            PrintString str;
            if (_autoReconnectTimeout) {
                str.printf_P(PSTR(", reconnecting in %d ms"), _autoReconnectTimeout);
            }
            Logger_notice(F("Disconnected from MQTT server, reason: %s%s"), _reasonToString(reason), str.c_str());
            for(const auto &component: _components) {
                component->onDisconnect(reason);
            }
        }
        _topics.clear();
        _resetClient();

        // reconnect automatically
        autoReconnect(_autoReconnectTimeout);
    }

    bool MQTT::Client::_topicInUse(ComponentPtr component, const String &topic)
    {
        for(const auto &mqttTopic: _topics) {
            if ((component != mqttTopic.getComponent()) && mqttTopic.match(topic)) {
                return true;
            }
        }
        return false;
    }

    bool MQTT::Client::publishAutoDiscovery(RunFlags flags, AutoDiscovery::StatusCallback callback)
    {
        #if MQTT_AUTO_DISCOVERY
            _startAutoDiscovery = false;
            _autoDiscoveryRebroadcast.remove();

            if (AutoDiscovery::Queue::isEnabled() && !_components.empty()) {
                if (_autoDiscoveryQueue && (flags & RunFlags::ABORT_RUNNING)) { // abort running auto discovery if flag is set
                    _autoDiscoveryQueue.reset();
                }
                if (!_autoDiscoveryQueue) {
                    __LDBG_printf("starting auto discovery queue");
                    _autoDiscoveryQueue.reset(new AutoDiscovery::Queue(*this));
                    _autoDiscoveryQueue->setStatusCallback(callback);
                    _autoDiscoveryQueue->publish(flags);
                    return true;
                }
            }
            __LDBG_printf("publishAutoDiscovery false: enabled=%u empty=%u queue=%p", AutoDiscovery::Queue::isEnabled(), _components.empty(), _autoDiscoveryQueue.get());
        #endif
        return false;
    }

    #if MQTT_AUTO_DISCOVERY

        void MQTT::Client::updateAutoDiscoveryTimestamps(bool success)
        {
            using namespace MQTT::Json;

            _resetAutoDiscoveryInitialState();

            auto now = time(nullptr);
            if (!isTimeValid(now)) {
                now = std::max(_autoDiscoveryLastSuccess, _autoDiscoveryLastFailure) + 1;
            }

            if (success) {
                _autoDiscoveryLastSuccess = now;
            }
            else {
                _autoDiscoveryLastFailure = now;
            }

            auto json = UnnamedObject(
                NamedUint32(F("last_success"), _autoDiscoveryLastSuccess),
                NamedUint32(F("last_failure"), _autoDiscoveryLastFailure)
            ).toString();
            publish(_autoDiscoveryStatusTopic, true, json, QosType::AT_LEAST_ONCE);
        }

    #endif

    void MQTT::Client::onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
    {
        __LDBG_printf("topic=%s payload=%s idx:len:total=%d:%d:%d qos=%d dup=%d retain=%d", topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), index, len, total, properties.qos, properties.dup, properties.retain);
        if (total > MQTT_RECV_MAX_MESSAGE_SIZE) {
            __LDBG_printf("discarding message, size exceeded %d/%d", (unsigned)total, MQTT_RECV_MAX_MESSAGE_SIZE);
            return;
        }
        if (index == 0) {
            if (_buffer.length()) {
                __LDBG_printf("discarding previous message, buffer=%u", _buffer.length());
                _buffer.clear();
            }
            _buffer.write(payload, len);
        } else if (index > 0) {
            if (index != _buffer.length()) {
                __LDBG_printf("discarding message, index=%u buffer=%u", index, _buffer.length());
                _buffer.clear();
                return;
            }
            _buffer.write(payload, len);
        }
        if (_buffer.length() == total) {
            onMessage(topic, _buffer.begin_str(), properties, total);
            _buffer.clear();
        }
    }

    void MQTT::Client::onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len)
    {
        __LDBG_printf("topic=%s payload=%s len=%d qos=%d dup=%d retain=%d", topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), len, properties.qos, properties.dup, properties.retain);
        _messageProperties = &properties;
        if (_autoDiscoveryStatusTopic == topic) {
            if (_autoDiscoveryLastFailure == ~0U || _autoDiscoveryLastSuccess == ~0U) {
                _resetAutoDiscoveryInitialState();
                char *ptr;
                ptr = strstr_P(payload, PSTR("failure\":"));
                if (ptr) {
                    ptr += 9;
                    uint32_t time = atol(ptr);
                    if (time) {
                        _autoDiscoveryLastFailure = time;
                    }
                }
                ptr = strstr_P(payload, PSTR("success\":"));
                if (ptr) {
                    ptr += 9;
                    uint32_t time = atol(ptr);
                    if (time) {
                        _autoDiscoveryLastSuccess = time;
                    }
                }
            }
        }
        else if (_lastWillTopic == topic) {
            // ignore this topic if not connected
            if (isConnected()) {
                auto onlineStr = MQTT_LAST_WILL_TOPIC_ONLINE;
                auto onlineLen = strlen_P(onlineStr);
                __LDBG_printf("online=%u(%s) len=%u(%s) cmp=%d", onlineLen, onlineStr, len, payload, strncmp_P(payload, onlineStr, std::min(len, onlineLen)));
                // if the length or contents do not match "online", publish "online" again
                // this happens if another client (or last will / dead connection) sets the status to "offline"
                if (onlineLen != len || strncmp_P(payload, onlineStr, len) != 0) {
                    __LDBG_printf("resending MQTT_LAST_WILL_TOPIC_ONLINE");
                    publish(_lastWillTopic, true, FPSTR(MQTT_LAST_WILL_TOPIC_ONLINE), QosType::ONLINE_STATUS);
                }
            }
        }
        else {
            for(const auto &mqttTopic : _topics) {
                if (_isTopicMatch(topic, mqttTopic.getTopic().c_str())) {
                    mqttTopic.getComponent()->onMessage(topic, payload, len);
                }
            }
        }
        _messageProperties = nullptr;
    }

    void MQTT::Client::_handleWiFiEvents(WiFiCallbacks::EventType event, void *payload)
    {
        __LDBG_printf("event=%d payload=%p conn=%s", event, payload, _connection());
        if (event == WiFiCallbacks::EventType::CONNECTED) {
            setAutoReconnect(_config.auto_reconnect_min);
            if (!isConnected()) {
                connect();
            }
        }
        else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
            // force a quick disconnect if WiFi is down and do not reconnect until it is back
            setAutoReconnect(kAutoReconnectDisabled);
            if (isConnected()) {
                _client->setKeepAlive(5);
            }
        }
    }

}

