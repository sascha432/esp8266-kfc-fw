/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include "mqtt_client.h"
#include "logger.h"
#include "plugins.h"
#include "mqtt_plugin.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

DEFINE_ENUM(MQTTQueueEnum_t);

MQTTClient *MQTTClient::_mqttClient = nullptr;

void MQTTClient::setupInstance()
{
    __LDBG_printf("enabled=%u", System::Flags::getConfig().is_mqtt_enabled);
    deleteInstance();
    if (System::Flags::getConfig().is_mqtt_enabled) {
        _mqttClient = new MQTTClient();
    }
}

void MQTTClient::deleteInstance()
{
    __LDBG_printf("client=%p", _mqttClient);
    if (_mqttClient) {
        delete _mqttClient;
        _mqttClient = nullptr;
    }
}

MQTTClient::MQTTClient() : _client(new AsyncMqttClient()), _componentsEntityCount(0), _lastWillPayload('0')
{
    _hostname = ClientConfig::getHostname();
    _username = ClientConfig::getUsername();
    _password = ClientConfig::getPassword();
    _config = ClientConfig::getConfig();
    _port = _config.getPort();

    __LDBG_printf("hostname=%s port=%u", _hostname.c_str(), _port);

    if (config.hasZeroConf(_hostname)) {
        config.resolveZeroConf(MQTTPlugin::getPlugin().getFriendlyName(), _hostname, _port, [this](const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type) {
            this->_zeroConfCallback(hostname, address, port, type);
        });
    }
    else {
        _zeroConfCallback(_hostname, convertToIPAddress(_hostname), _port, MDNSResolver::ResponseType::NONE);
    }
}

MQTTClient::~MQTTClient()
{
    __LDBG_printf("connnected=%u", isConnected());
    WiFiCallbacks::remove(WiFiCallbacks::EventType::CONNECTION, MQTTClient::handleWiFiEvents);
    _clearQueue();
    _timer.remove();

    _autoReconnectTimeout = 0;
    if (isConnected()) {
        disconnect(true);
        onDisconnect(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
    }
    delete _client;
}

void MQTTClient::_zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
{
    _address = address;
    _hostname = hostname;
    _port = port;
    __LDBG_printf("zeroconf address=%s hostname=%s port=%u type=%u", _address.toString().c_str(), _hostname.c_str(), _port, type);

    _setupClient();
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, MQTTClient::handleWiFiEvents);

    if (WiFi.isConnected()) {
        connect();
    }

}

void MQTTClient::_setupClient()
{
    _debug_println();
    _clearQueue();

    _autoReconnectTimeout = MQTT_AUTO_RECONNECT_TIMEOUT;

    if (_address.isSet()) {
        _client->setServer(_address, _port);
    }
    else {
        _client->setServer(_hostname.c_str(), _port);
    }
#if ASYNC_TCP_SSL_ENABLED
    if (Config_MQTT::getMode() == MQTT_MODE_SECURE) {
        _client->setSecure(true);
        auto fingerPrint = Config_MQTT::getFingerprint();
        if (fingerPrint && *fingerPrint) {
            _client->addServerFingerprint(fingerPrint); // addServerFingerprint supports multiple fingerprints
        }
    }
#endif
    _client->setCredentials(_username.c_str(), _password.c_str());
    _client->setKeepAlive(_config.keepalive);

    _client->onConnect([this](bool sessionPresent) {
        onConnect(sessionPresent);
    });
    _client->onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
        onDisconnect(reason);
    });
    _client->onMessage([this](char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        onMessageRaw(topic, payload, properties, len, index, total);
    });
#if MQTT_USE_PACKET_CALLBACKS
    _client->onPublish([this](uint16_t packetId) {
        onPublish(packetId);
    });
    _client->onSubscribe([this](uint16_t packetId, uint8_t qos) {
        onSubscribe(packetId, qos);
    });
    _client->onUnsubscribe([this](uint16_t packetId) {
        onUnsubscribe(packetId);
    });
#endif
}

void MQTTClient::registerComponent(MQTTComponentPtr component)
{
    _debug_printf_P(PSTR("component=%p\n"), component);
    if (unregisterComponent(component)) {
        debug_printf_P(PSTR("component registered multiple times: name=%s component=%p\n"), component->getName(), component);
    }
    _components.emplace_back(component);
    _componentsEntityCount += component->getAutoDiscoveryCount();
    _debug_printf_P(PSTR("components=%u entities=%u\n"), _components.size(), _componentsEntityCount);
}

bool MQTTClient::unregisterComponent(MQTTComponentPtr component)
{
    _debug_printf_P(PSTR("component=%p components=%u entities=%u\n"), component, _components.size(), _componentsEntityCount);
    bool removed = false;
    if (_components.size()) {
        remove(component);
        auto iterator = std::remove(_components.begin(), _components.end(), component);
        if (iterator != _components.end()) {
            _componentsEntityCount -= component->getAutoDiscoveryCount();
            _components.erase(iterator, _components.end());
            removed = true;
        }
    }
    _debug_printf_P(PSTR("components=%u entities=%u removed=%u\n"), _components.size(), _componentsEntityCount, removed);
    return removed;
}

bool MQTTClient::isComponentRegistered(MQTTComponentPtr component)
{
    return std::find(_components.begin(), _components.end(), component) != _components.end();
}

String MQTTClient::formatTopic(const String &componentName, const __FlashStringHelper *format, ...)
{
    String suffix;
    if (componentName.length()) {
        if (componentName.charAt(0) != '/') { // avoid double slash
            suffix = '/';
        }
        suffix += componentName;
    }
    va_list arg;
    va_start(arg, format);
    String topic = _formatTopic(suffix, format, arg);
    va_end(arg);
    return topic;
}

String MQTTClient::_formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg)
{
    PrintString topic;
    topic = ClientConfig::getTopic();
    topic.replace(F("${device_name}"), System::Device::getName());
    topic.print(suffix);
    if (format) {
        auto format_P = RFPSTR(format);
        // if (pgm_read_byte(format_P) == '/' && String_endsWith(topic, '/')) { // avoid double slash
        //     format_P++;
        // }
        topic.vprintf_P(format_P, arg);
    }
    _debug_printf_P(PSTR("topic=%s\n"), topic.c_str());
    return topic;
}

bool MQTTClient::isConnected() const
{
    return _client->connected();
}

void MQTTClient::setAutoReconnect(uint32_t timeout)
{
    _debug_printf_P(PSTR("timeout=%d\n"), timeout);
    _autoReconnectTimeout = timeout;
}

void MQTTClient::connect()
{
    _debug_printf_P(PSTR("status=%s connected=%u\n"), connectionStatusString().c_str(), _client->connected());

    // remove reconnect timer if running and force disconnect
    auto timeout = _autoReconnectTimeout;
    _timer.remove();

    if (isConnected()) {
        _autoReconnectTimeout = 0; // disable auto reconnect
        disconnect(true);
        _autoReconnectTimeout = timeout;
    }

    _client->setCleanSession(true);

    setLastWill('0');

    _client->connect();
//    autoReconnect(_autoReconnectTimeout);
}

void MQTTClient::disconnect(bool forceDisconnect)
{
    _debug_printf_P(PSTR("forceDisconnect=%d status=%s connected=%u\n"), forceDisconnect, connectionStatusString().c_str(), _client->connected());
    if (!forceDisconnect) {
        // set offline
        publish(_lastWillTopic, true, String(0));
    }
    _client->disconnect(forceDisconnect);
}

void MQTTClient::setLastWill(char value)
{
    // set last will to mark component as offline on disconnect
    // last will topic and payload need to be static
    if (value) {
        _lastWillPayload = value;
    }

    _lastWillTopic = formatTopic(FSPGM(mqtt_status_topic));
    _debug_printf_P(PSTR("topic=%s value=%s\n"), _lastWillTopic.c_str(), _lastWillPayload.c_str());
#if MQTT_SET_LAST_WILL
    _client->setWill(_lastWillTopic.c_str(), getDefaultQos(), true, _lastWillPayload.c_str(), _lastWillPayload.length());
#endif
}

void MQTTClient::autoReconnect(uint32_t timeout)
{
    _debug_printf_P(PSTR("timeout=%u\n"), timeout);
    if (timeout) {
        _timer.add(timeout, false, [this](EventScheduler::TimerPtr timer) {
            _debug_printf_P(PSTR("isConnected=%d\n"), this->isConnected());
            if (!this->isConnected()) {
                this->connect();
            }
        });

        // increase timeout on each reconnect attempt
        setAutoReconnect(timeout + timeout * 0.3);
    }
}

uint8_t MQTTClient::getDefaultQos(uint8_t qos)
{
    if (qos != QOS_DEFAULT) {
        return qos;
    }
    if (_mqttClient) {
        return _mqttClient->_config.qos;
    }
    return ClientConfig::getConfig().qos;
}

String MQTTClient::connectionDetailsString()
{
    auto message = PrintString(F("%s@%s:%u"), _username.length() ? _username.c_str() : SPGM(Anonymous), (_address.isSet() ? _address.toString().c_str() : _hostname.c_str()), _port);
#if ASYNC_TCP_SSL_ENABLED
    if (Config_MQTT::getMode() == MQTT_MODE_SECURE) {
        message += F(", Secure MQTT");
    }
#endif
    return message;
}

String MQTTClient::connectionStatusString()
{
    String message = connectionDetailsString();
    if (isConnected()) {
        message += F(", connected, ");
    } else {
        message += F(", disconnected, ");
    }
    message += F("topic ");

    message += formatTopic(emptyString);
#if MQTT_AUTO_DISCOVERY
    if (_config.auto_discovery) {
        message += F(HTML_S(br) "Auto discovery prefix '");
        message += ClientConfig::getAutoDiscoveryPrefix();
        message += '\'';
    }
#endif
    return message;
}

void MQTTClient::onConnect(bool sessionPresent)
{
    _debug_printf_P(PSTR("session=%d\n"), sessionPresent);
    Logger_notice(F("Connected to MQTT server %s"), connectionDetailsString().c_str());

    _clearQueue();

    // set online
    publish(_lastWillTopic, true, String(1));

    // reset reconnect timer if connection was successful
    setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);

    for(const auto &component: _components) {
        component->onConnect(this);
    }
    publishAutoDiscovery();
}

void MQTTClient::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    _debug_printf_P(PSTR("reason=%d auto_reconnect=%d\n"), (int)reason, _autoReconnectTimeout);
    PrintString str;
    if (_autoReconnectTimeout) {
        str.printf_P(PSTR(", reconnecting in %d ms"), _autoReconnectTimeout);
    }
    Logger_notice(F("Disconnected from MQTT server, reason: %s%s"), _reasonToString(reason), str.c_str());
    for(const auto &component: _components) {
        component->onDisconnect(this, reason);
    }
    _topics.clear();
    _clearQueue();

    // reconnect automatically
    autoReconnect(_autoReconnectTimeout);
}

void MQTTClient::subscribe(MQTTComponentPtr component, const String &topic, uint8_t qos)
{
    qos = MQTTClient::getDefaultQos(qos);
    if (!_queue.empty() || subscribeWithId(component, topic, qos) == 0) {
        _addQueue(MQTTQueueType::SUBSCRIBE, component, topic, qos, 0, String());
    }
}

int MQTTClient::subscribeWithId(MQTTComponentPtr component, const String &topic, uint8_t qos)
{
    qos = MQTTClient::getDefaultQos(qos);
#if DEBUG
    if (topic.length() == 0) {
        __debugbreak_and_panic_printf_P(PSTR("subscribeWithId: topic is empty\n"));
    }
#endif
    _debug_printf_P(PSTR("component=%p topic=%s qos=%u\n"), component, topic.c_str(), qos);
    auto result = _client->subscribe(topic.c_str(), qos);
    if (result && component) {
        _topics.emplace_back(topic, component);
    }
    return result;
}

void MQTTClient::unsubscribe(MQTTComponentPtr component, const String &topic)
{
    if (!_queue.empty() || unsubscribeWithId(component, topic) == 0) {
        _addQueue(MQTTQueueType::UNSUBSCRIBE, component, topic, 0, 0, String());
    }
}

int MQTTClient::unsubscribeWithId(MQTTComponentPtr component, const String &topic)
{
    int result = -1;
    _debug_printf_P(PSTR("component=%p topic=%s in_use=%d\n"), component, topic.c_str(), _topicInUse(component, topic));
    if (!_topicInUse(component, topic)) {
        result = _client->unsubscribe(topic.c_str());
    }
    if (result && component) {
        _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [component, topic](const MQTTTopic &mqttTopic) {
            if (mqttTopic.match(component, topic)) {
                return true;
            }
            return false;
        }), _topics.end());
    }
    return result;
}

void MQTTClient::remove(MQTTComponentPtr component)
{
    _debug_printf_P(PSTR("component=%p topics=%u\n"), component, _topics.size());
    _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [this, component](const MQTTTopic &mqttTopic) {
        if (mqttTopic.getComponent() == component) {
            _debug_printf_P(PSTR("topic=%s in_use=%d\n"), mqttTopic.getTopic().c_str(), _topicInUse(component, mqttTopic.getTopic()));
            if (!_topicInUse(component, mqttTopic.getTopic())) {
                unsubscribe(nullptr, mqttTopic.getTopic().c_str());
            }
            return true;
        }
        return false;
    }), _topics.end());
}

bool MQTTClient::_topicInUse(MQTTComponentPtr component, const String &topic)
{
    for(const auto &mqttTopic: _topics) {
        if (mqttTopic.match(component, topic)) {
            return true;
        }
    }
    return false;
}

void MQTTClient::publish(const String &topic, bool retain, const String &payload, uint8_t qos)
{
    qos = MQTTClient::getDefaultQos(qos);
    if (publishWithId(topic, retain, payload, qos) == 0) {
        _addQueue(MQTTQueueType::PUBLISH, nullptr, topic, qos, retain, payload);
    }
}

bool MQTTClient::publishAutoDiscovery()
{
#if MQTT_AUTO_DISCOVERY
    _autoDiscoveryRebroadcast.remove();

    if (MQTTAutoDiscovery::isEnabled() && !_components.empty()) {
        if (_autoDiscoveryQueue) {
            __DBG_printf("auto discovery running count=%u size=%u", _autoDiscoveryQueue->_discoveryCount, _autoDiscoveryQueue->_size);
        }
        else {
            _autoDiscoveryQueue.reset(new MQTTAutoDiscoveryQueue(*this));
            _autoDiscoveryQueue->publish();
            return true;
        }
    }
#endif
    return false;
}

void MQTTClient::publishAutoDiscoveryCallback(EventScheduler::TimerPtr timer)
{
    if (_mqttClient) {
        _mqttClient->publishAutoDiscovery();
    }
}

int MQTTClient::publishWithId(const String &topic, bool retain, const String &payload, uint8_t qos)
{
    qos = MQTTClient::getDefaultQos(qos);
    _debug_printf_P(PSTR("topic=%s qos=%u retain=%d payload=%s\n"), topic.c_str(), qos, retain, printable_string(payload.c_str(), payload.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
    return _client->publish(topic.c_str(), qos, retain, payload.c_str(), payload.length());
}

void MQTTClient::onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    _debug_printf_P(PSTR("topic=%s payload=%s idx:len:total=%d:%d:%d qos=%u dup=%d retain=%d\n"), topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), index, len, total, properties.qos, properties.dup, properties.retain);
    if (total > MQTT_RECV_MAX_MESSAGE_SIZE) {
        _debug_printf(PSTR("discarding message, size exceeded %d/%d\n"), (unsigned)total, MQTT_RECV_MAX_MESSAGE_SIZE);
        return;
    }
    if (index == 0) {
        if (_buffer.length()) {
            _debug_printf(PSTR("discarding previous message, buffer=%u\n"), _buffer.length());
            _buffer.clear();
        }
        _buffer.write(payload, len);
    } else if (index > 0) {
        if (index != _buffer.length()) {
            _debug_printf(PSTR("discarding message, index=%u buffer=%u\n"), index, _buffer.length());
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

void MQTTClient::onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len)
{
    _debug_printf_P(PSTR("topic=%s payload=%s len=%d qos=%u dup=%d retain=%d\n"), topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), len, properties.qos, properties.dup, properties.retain);
    for(const auto &mqttTopic : _topics) {
        if (_isTopicMatch(topic, mqttTopic.getTopic().c_str())) {
            mqttTopic.getComponent()->onMessage(this, topic, payload, len);
        }
    }
}

#if MQTT_USE_PACKET_CALLBACKS

void MQTTClient::onPublish(uint16_t packetId)
{
    _debug_printf_P(PSTR("MQTTClient::onPublish(%u)\n"), packetId);
}

void MQTTClient::onSubscribe(uint16_t packetId, uint8_t qos)
{
    _debug_printf_P(PSTR("MQTTClient::onSubscribe(%u, %u)\n"), packetId, qos);
}

void MQTTClient::onUnsubscribe(uint16_t packetId)
{
    _debug_printf_P(PSTR("MQTTClient::onUnsubscribe(%u)\n"), packetId);
}

#endif

bool MQTTClient::_isTopicMatch(const char *topic, const char *match) const
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
                } else if (*topic != *match) {
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

const __FlashStringHelper *MQTTClient::_reasonToString(AsyncMqttClientDisconnectReason reason) const
{
    switch(reason) {
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

void MQTTClient::_handleWiFiEvents(WiFiCallbacks::EventType event, void *payload)
{
    _debug_printf_P(PSTR("event=%d payload=%p connected=%d\n"), event, payload, isConnected());
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        if (!isConnected()) {
            setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);
            connect();
        }
    } else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        if (isConnected()) {
            setAutoReconnect(0);
            disconnect(true);
        }
    }
}

void MQTTClient::_addQueue(MQTTQueueEnum_t type, MQTTComponent *component, const String &topic, uint8_t qos, uint8_t retain, const String &payload)
{
    _debug_printf_P(PSTR("type=%s topic=%s\n"), type.toString().c_str(), topic.c_str());

    if (MQTTClient::_isMessageSizeExceeded(topic.length() + payload.length(), topic.c_str())) {
        return;
    }

    _queue.emplace_back(type, component, topic, qos, retain, payload);
    _queueTimer.add(MQTT_QUEUE_RETRY_DELAY, (MQTT_QUEUE_TIMEOUT / MQTT_QUEUE_RETRY_DELAY), [this](EventScheduler::TimerPtr timer) {
        _queueTimerCallback();
    });
}

void MQTTClient::_queueTimerCallback()
{
    // process queue
    while(_queue.size()) {
        int result = -1;
        auto queue = _queue.front();
        _debug_printf_P(PSTR("queue=%u topic=%s space=%u\n"), _queue.size(), queue.getTopic().c_str(), getClientSpace());
        switch(queue.getType()) {
            case MQTTQueueType::SUBSCRIBE:
                result = subscribeWithId(queue.getComponent(), queue.getTopic(), queue.getQos());
                break;
            case MQTTQueueType::UNSUBSCRIBE:
                result = unsubscribeWithId(queue.getComponent(), queue.getTopic());
                break;
            case MQTTQueueType::PUBLISH:
                result = publishWithId(queue.getTopic(), queue.getRetain(), queue.getPayload(), queue.getQos());
                break;
        }
        if (result == 0) {
            return; // failure, retry again later
        }
        _queue.erase(_queue.begin());
    }

    _debug_println(F("queue done"));
    _queueTimer.remove();
}

void MQTTClient::_clearQueue()
{
    _autoDiscoveryQueue.reset();
    _autoDiscoveryRebroadcast.remove();
    _queueTimer.remove();
    _queue.clear();
}

size_t MQTTClient::getClientSpace() const
{
    return _client->getAsyncClient().space();
}

bool MQTTClient::_isMessageSizeExceeded(size_t len, const char *topic)
{
    if (len > MQTT_MAX_MESSAGE_SIZE) {
        Logger_error(F("MQTT maximum message size exceeded: %u/%u: %s"), len, MQTT_MAX_MESSAGE_SIZE, topic);
        return true;
    }
    return false;
}
