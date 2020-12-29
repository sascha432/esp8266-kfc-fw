/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <EventScheduler.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "mqtt_strings.h"
#include "mqtt_client.h"
#include "logger.h"
#include "plugins.h"
#include "mqtt_plugin.h"
#include "templates.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

using KFCConfigurationClasses::System;

MQTTClient *MQTTClient::_mqttClient = nullptr;

void MQTTClient::setupInstance()
{
    __LDBG_printf("enabled=%u", System::Flags::getConfig().is_mqtt_enabled);
    deleteInstance();
    if (System::Flags::getConfig().is_mqtt_enabled) {
        _mqttClient = __LDBG_new(MQTTClient);
    }
}

void MQTTClient::deleteInstance()
{
    __LDBG_printf("client=%p", _mqttClient);
    if (_mqttClient) {
        __LDBG_delete(_mqttClient);
        _mqttClient = nullptr;
    }
}

#include <HeapStream.h>

MQTTClient::MQTTClient() :
    _hostname(ClientConfig::getHostname()),
    _username(ClientConfig::getUsername()),
    _password(ClientConfig::getPassword()),
    _config(ClientConfig::getConfig()),
    _client(__LDBG_new(AsyncMqttClient)),
    _port(_config.getPort()),
    _componentsEntityCount(0),
    _lastWillPayload('0'),
    _connState(ConnectionState::NONE)
{
    constexpr size_t clientIdLength = 18;
    char *clientIdBuffer = const_cast<char *>(_client->getClientId()); // reuse the existing buffer that is declared private
    WriteableHeapStream clientId(clientIdBuffer, clientIdLength + 1); // buffer size, max. allowed size is 23
    clientId.print(F("kfc-"));
    WebTemplate::printUniqueId(clientId, FSPGM(kfcfw));
    clientIdBuffer[clientIdLength] = 0;

    __LDBG_printf("hostname=%s port=%u client_id=%s", _hostname.c_str(), _port, clientIdBuffer);

    if (config.hasZeroConf(_hostname)) {
        config.resolveZeroConf(MQTTPlugin::getPlugin().getFriendlyName(), _hostname, _port, [this](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) {
            this->_zeroConfCallback(hostname, address, port, type);
        });
    }
    else {
        _zeroConfCallback(_hostname, convertToIPAddress(_hostname), _port, MDNSResolver::ResponseType::NONE);
    }
}

MQTTClient::~MQTTClient()
{
    __LDBG_printf("conn=%s", _connection());
    WiFiCallbacks::remove(WiFiCallbacks::EventType::CONNECTION, MQTTClient::handleWiFiEvents);
    _clearQueue();
    _timer.remove();

    _autoReconnectTimeout = kAutoReconnectDisabled;
    disconnect(true);
    onDisconnect(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
    __LDBG_delete(_client);
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

    _autoReconnectTimeout = _config.auto_reconnect_min;

    if (_address.isSet()) {
        _client->setServer(_address, _port);
    }
    else {
        _client->setServer(_hostname.c_str(), _port);
    }
#if ASYNC_TCP_SSL_ENABLED
    if (ConfigType::cast_enum_mode(_config.mode) == ModeType::SECURE) {
        _client->setSecure(true);
        uint16_t size = ClientConfig::kFingerprintMaxSize;
        auto fingerPrint = ClientConfig::getFingerPrint(size);
        if (fingerPrint && size == ClientConfig::kFingerprintMaxSize) {
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
        onSubscribe(packetId, static_cast<QosType>(qos));
    });
    _client->onUnsubscribe([this](uint16_t packetId) {
        onUnsubscribe(packetId);
    });
#elif DEBUG_MQTT_CLIENT
    _client->onPublish([this](uint16_t packetId) {
        __LDBG_printf("packet_id=%u", packetId);
    });
    _client->onSubscribe([this](uint16_t packetId, uint8_t qos) {
        __LDBG_printf("packet_id=%u qos=%u", packetId, qos);
    });
    _client->onUnsubscribe([this](uint16_t packetId) {
        __LDBG_printf("packet_id=%u", packetId);
    });
#endif
}

void MQTTClient::registerComponent(ComponentPtr component)
{
    __LDBG_printf("component=%p", component);
    if (unregisterComponent(component)) {
        debug_printf_P(PSTR("component registered multiple times: name=%s component=%p\n"), component->getName(), component);
    }
    _components.emplace_back(component);
    _componentsEntityCount += component->getAutoDiscoveryCount();
    __LDBG_printf("components=%u entities=%u", _components.size(), _componentsEntityCount);
}

bool MQTTClient::unregisterComponent(ComponentPtr component)
{
    __LDBG_printf("component=%p components=%u entities=%u", component, _components.size(), _componentsEntityCount);
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
    __LDBG_printf("components=%u entities=%u removed=%u", _components.size(), _componentsEntityCount, removed);
    return removed;
}

bool MQTTClient::isComponentRegistered(ComponentPtr component)
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

String MQTTClient::_filterString(const char *str, bool replaceSpace)
{
    String out;
    out.reserve(strlen(str));
    while(*str) {
        if (*str == '#' || *str == '+' || *str == '/') {
        }
        else if (replaceSpace && *str == ' ') {
            out += '_';
        }
        else if (isprint(*str)) {
            out += *str;
        }
        str++;
    }
    return out;
}

String MQTTClient::_getBaseTopic()
{
    PrintString topic;
    topic = ClientConfig::getTopic();
    if (topic.indexOf(F("${device_name}")) != -1) {
        topic.replace(F("${device_name}"), _filterString(System::Device::getName()));
    }
    if (topic.indexOf(F("${device_title}")) != -1) {
        topic.replace(F("${device_title}"), _filterString(System::Device::getTitle()));
    }
    if (topic.indexOf(F("${device_title_no_space}")) != -1) {
        topic.replace(F("${device_title_no_space}"), _filterString(System::Device::getTitle(), true));
    }
    return topic;
}

#if MQTT_GROUP_TOPIC

bool MQTTClient::_getGroupTopic(ComponentPtr component, String groupTopic, String &topic)
{
    String deviceTopic = ClientConfig::getTopic();
    if (!topic.startsWith(deviceTopic) || topic.startsWith(groupTopic)) {
        return false;
    }
    if (topic.indexOf(F("${component_name}")) != -1) {
        topic.replace(F("${component_name}"), component->getName());
    }
    groupTopic += &topic.c_str()[deviceTopic.length() - 1];
    topic = std::move(groupTopic);
    return true;
}

#endif

String MQTTClient::_formatTopic(const String &suffix, const __FlashStringHelper *format, va_list arg)
{
    PrintString topic = ClientConfig::getTopic();
    topic.print(suffix);
    if (format) {
        auto format_P = RFPSTR(format);
        // if (pgm_read_byte(format_P) == '/' && String_endsWith(topic, '/')) { // avoid double slash
        //     format_P++;
        // }
        topic.vprintf_P(format_P, arg);
    }
    __LDBG_printf("topic=%s", topic.c_str());
    return topic;
}

#if DEBUG_MQTT_CLIENT

const __FlashStringHelper *MQTTClient::_getConnStateStr()
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

const char *MQTTClient::_connection()
{
    DEBUG_HELPER_PUSH_STATE();
    DEBUG_HELPER_SILENT();
    PrintString str(F("%s state=%s auto_reconnect=%u timer=%u"), _client->getAsyncClient().stateToString(), RFPSTR(_getConnStateStr()), _autoReconnectTimeout, (bool)_timer);
    _connectionStr = std::move(str);
    DEBUG_HELPER_POP_STATE();
    return _connectionStr.c_str();
}

#endif

void MQTTClient::connect()
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
    _timer.remove();

    _autoReconnectTimeout = kAutoReconnectDisabled; // disable auto reconnect
    disconnect(true);
    _autoReconnectTimeout = timeout;

    _client->setCleanSession(true);
    _client->setKeepAlive(_config.keepalive);

    setLastWillPayload('0');
    publishLastWill();

    _connState = ConnectionState::CONNECTING;
    _client->connect();
}

void MQTTClient::disconnect(bool forceDisconnect)
{
    __LDBG_printf("force_disconnect=%d conn=%s", forceDisconnect, _connection());
#if MQTT_SET_LAST_WILL == 0
    if (setConnState(ConnectionState::DISCONNECTING) == ConnectionState::CONNECTED && !forceDisconnect && _client->connected()) {
        // set offline if last will is not used
        // for example for clients that go to deep sleep
        // when reconfiguring or stopping mqtt, the device is marked offline manually
        __LDBG_printf("marking device as offline manually conn=%s", _connection());
        _clearQueue();
        publishWithId(_lastWillTopic, true, String(0));
    }
#else
    _connState = ConnectionState::DISCONNECTING;
#endif
    _client->disconnect(forceDisconnect);
}

void MQTTClient::publishLastWill()
{
#if MQTT_SET_LAST_WILL
    __LDBG_printf("topic=%s value=%s", _lastWillTopic.c_str(), _lastWillPayload.c_str());
    if (_lastWillTopic.length()) {
        _client->setWill(_lastWillTopic.c_str(), _translateQosType(getDefaultQos()), true, _lastWillPayload.c_str(), _lastWillPayload.length());
    }
#else
    __LDBG_printf("topic=%s value=%s (not sent to server)", _lastWillTopic.c_str(), _lastWillPayload.c_str());
#endif
}

void MQTTClient::autoReconnect(AutoReconnectType timeout)
{
    __LDBG_printf("timeout=%u conn=%s", timeout, _connection());
    if (timeout) {
        _connState = ConnectionState::AUTO_RECONNECT_DELAY;
        _Timer(_timer).add(timeout, false, [this](Event::CallbackTimerPtr timer) {
            __LDBG_printf("timer lambda: conn=%s", _connection());
            if (!this->isConnected()) {
                this->connect();
            }
        });
        // increase timeout on each reconnect attempt
        setAutoReconnect(std::min<AutoReconnectType>(timeout + (static_cast<uint32_t>(timeout) * _config.auto_reconnect_incr / 100U), _config.auto_reconnect_max));

// # python code to print the timeouts
// to=5000
// max_to=60000
// incr=10
// tosum=0
// for i in range(1, 1000):
//     print('retry=%u timeout=%u total_time=%u (%.2f min)' % (i, to, tosum, tosum / 60000))
//     tosum += to
//     if to==max_to:
//         break
//     to=to+(to / 10)
//     to=int(to)
//     if to>max_to:
//         to=max_to
    }
    else {
        _connState = ConnectionState::IDLE;
    }
}

String MQTTClient::connectionDetailsString()
{
    auto message = PrintString(F("%s@%s:%u"), _username.length() ? _username.c_str() : SPGM(Anonymous), (_address.isSet() ? _address.toString().c_str() : _hostname.c_str()), _port);
#if ASYNC_TCP_SSL_ENABLED
    if (ConfigType::cast_enum_mode(_config.mode) == ModeType::SECURE) {
        message += F(", Secure MQTT");
    }
#endif
    return message;
}

String MQTTClient::connectionStatusString()
{
    String message = connectionDetailsString();
    switch(_connState) {
        case ConnectionState::AUTO_RECONNECT_DELAY:
            message += F(", waiting to reconnect, ");
            break;
        case ConnectionState::IDLE:
            message += F(", idle, ");
            break;
        case ConnectionState::PRE_CONNECT:
        case ConnectionState::CONNECTING:
            message += F(", connecting, ");
            break;
        case ConnectionState::CONNECTED:
            message += F(", connected, ");
            break;
        case ConnectionState::DISCONNECTED:
        case ConnectionState::DISCONNECTING:
        default:
            message += F(", disconnected, ");
            break;

    }
    // if (isConnected()) {
    //     message += F(", connected, ");
    // } else {
    //     message += F(", disconnected, ");
    // }
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
    __LDBG_printf("session=%d conn=%s", sessionPresent, _connection());
    Logger_notice(F("Connected to MQTT server %s"), connectionDetailsString().c_str());
    _connState = ConnectionState::CONNECTED;

    _clearQueue();

    // set online status
    publish(_lastWillTopic, true, String(1));

    // reset reconnect timer if connection was successful
    setAutoReconnect(_config.auto_reconnect_min);

    for(const auto &component: _components) {
        component->onConnect(this);
    }
    publishAutoDiscovery();
}

void MQTTClient::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("reason=%d auto_reconnect=%d conn=%s", (int)reason, _autoReconnectTimeout, _connection());
    // check if the connection was established
    if (setConnState(ConnectionState::DISCONNECTED) == ConnectionState::CONNECTED) {
        PrintString str;
        if (_autoReconnectTimeout) {
            str.printf_P(PSTR(", reconnecting in %d ms"), _autoReconnectTimeout);
        }
        Logger_notice(F("Disconnected from MQTT server, reason: %s%s"), _reasonToString(reason), str.c_str());
        for(const auto &component: _components) {
            component->onDisconnect(this, reason);
        }
    }
    _topics.clear();
    _clearQueue();

    // reconnect automatically
    autoReconnect(_autoReconnectTimeout);
}

void MQTTClient::subscribe(ComponentPtr component, const String &topic, QosType qos)
{
    if (!_queue.empty() || subscribeWithId(component, topic, qos) == 0) {
        _addQueue(QueueType::SUBSCRIBE, component, topic, qos, 0, String());
    }
}

int MQTTClient::subscribeWithId(ComponentPtr component, const String &topic, QosType qos)
{
#if DEBUG
    if (topic.length() == 0) {
        __DBG_panic("subscribeWithId: topic is empty");
    }
#endif
    __LDBG_printf("component=%p topic=%s qos=%u conn=%s", component, topic.c_str(), qos, _connection());
    auto result = _client->subscribe(topic.c_str(), MQTTClient::_translateQosType(qos));
    if (result && component) {
        _topics.emplace_back(topic, component);
    }
    return result;
}

void MQTTClient::unsubscribe(ComponentPtr component, const String &topic)
{
    if (!_queue.empty() || unsubscribeWithId(component, topic) == 0) {
        _addQueue(QueueType::UNSUBSCRIBE, component, topic, getDefaultQos(), 0, String());
    }
}

#if MQTT_GROUP_TOPIC

void MQTTClient::subscribeWithGroup(ComponentPtr component, const String &topic, QosType qos)
{
    if (!_queue.empty() || subscribeWithId(component, topic, qos) == 0) {
        _addQueue(QueueType::SUBSCRIBE, component, topic, qos, 0, String());
    }
    auto groupBaseTopic = ClientConfig::getGroupTopic();
    if (*groupBaseTopic) {
        String groupTopic;
        if (_getGroupTopic(component, groupBaseTopic, groupTopic)) {
            if (!_queue.empty() || subscribeWithId(component, groupTopic, qos) == 0) {
                _addQueue(QueueType::SUBSCRIBE, component, groupTopic, qos, 0, String());
            }
        }
    }
}

void MQTTClient::unsubscribeWithGroup(ComponentPtr component, const String &topic)
{
    if (!_queue.empty() || unsubscribeWithId(component, topic) == 0) {
        _addQueue(QueueType::UNSUBSCRIBE, component, topic, getDefaultQos(), 0, String());
    }
    auto groupBaseTopic = ClientConfig::getGroupTopic();
    if (*groupBaseTopic) {
        String groupTopic;
        if (_getGroupTopic(component, groupBaseTopic, groupTopic)) {
            if (!_queue.empty() || unsubscribeWithId(component, groupTopic) == 0) {
                _addQueue(QueueType::UNSUBSCRIBE, component, groupTopic, getDefaultQos(), 0, String());
            }
        }
    }
}

#endif

int MQTTClient::unsubscribeWithId(ComponentPtr component, const String &topic)
{
    int result = -1;
    __LDBG_printf("component=%p topic=%s in_use=%d conn=%s", component, topic.c_str(), _topicInUse(component, topic), _connection());
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

void MQTTClient::remove(ComponentPtr component)
{
    __LDBG_printf("component=%p topics=%u", component, _topics.size());
    _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [this, component](const MQTTTopic &mqttTopic) {
        if (mqttTopic.getComponent() == component) {
            __LDBG_printf("topic=%s in_use=%d", mqttTopic.getTopic().c_str(), _topicInUse(component, mqttTopic.getTopic()));
            if (!_topicInUse(component, mqttTopic.getTopic())) {
                unsubscribe(nullptr, mqttTopic.getTopic().c_str());
            }
            return true;
        }
        return false;
    }), _topics.end());
}

bool MQTTClient::_topicInUse(ComponentPtr component, const String &topic)
{
    for(const auto &mqttTopic: _topics) {
        if (mqttTopic.match(component, topic)) {
            return true;
        }
    }
    return false;
}

void MQTTClient::publish(const String &topic, bool retain, const String &payload, QosType qos)
{
    if (publishWithId(topic, retain, payload, qos) == 0) {
        _addQueue(QueueType::PUBLISH, nullptr, topic, qos, retain, payload);
    }
}

void MQTTClient::publishPersistantStorage(StorageFrequencyType type, const String &name, const String &data)
{
    time_t now = time(nullptr);
    if (!IS_TIME_VALID(now)) {
        return;
    }
    PrintString str = F("persistant_storage/");
    str += name;
    str += '/';
    switch(type) {
        case StorageFrequencyType::DAILY:
            str.strftime_P(PSTR("%Y%m%d"), now);
            break;
        case StorageFrequencyType::WEEKLY:
            str.strftime_P(PSTR("%Y%m-%W"), now);
            break;
        case StorageFrequencyType::MONTHLY:
            str.strftime_P(PSTR("%Y%m"), now);
            break;
    }
    publish(formatTopic(str), true, data, QosType::PERSISTENT_STORAGE);
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

int MQTTClient::publishWithId(const String &topic, bool retain, const String &payload, QosType qos)
{
    __LDBG_printf("topic=%s qos=%u retain=%d payload=%s conn=%s", topic.c_str(), qos, retain, printable_string(payload.c_str(), payload.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), _connection());
    return _client->publish(topic.c_str(), MQTTClient::_translateQosType(qos), retain, payload.c_str(), payload.length());
}

void MQTTClient::onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    __LDBG_printf("topic=%s payload=%s idx:len:total=%d:%d:%d qos=%u dup=%d retain=%d", topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), index, len, total, properties.qos, properties.dup, properties.retain);
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

void MQTTClient::onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len)
{
    __LDBG_printf("topic=%s payload=%s len=%d qos=%u dup=%d retain=%d", topic, printable_string(payload, len, DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str(), len, properties.qos, properties.dup, properties.retain);
    for(const auto &mqttTopic : _topics) {
        if (_isTopicMatch(topic, mqttTopic.getTopic().c_str())) {
            mqttTopic.getComponent()->onMessage(this, topic, payload, len);
        }
    }
}

#if MQTT_USE_PACKET_CALLBACKS

void MQTTClient::onPublish(uint16_t packetId)
{
    __LDBG_printf("MQTTClient::onPublish(%u)", packetId);
}

void MQTTClient::onSubscribe(uint16_t packetId, QosType qos)
{
    __LDBG_printf("MQTTClient::onSubscribe(%u, %u)", packetId, qos);
}

void MQTTClient::onUnsubscribe(uint16_t packetId)
{
    __LDBG_printf("MQTTClient::onUnsubscribe(%u)", packetId);
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
    __LDBG_printf("event=%d payload=%p conn=%s", event, payload, _connection());
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        if (!isConnected()) {
            setAutoReconnect(_config.auto_reconnect_min);
            connect();
        }
    } else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        // force a quick disconnect if WiFi is down
        if (isConnected()) {
            _client->setKeepAlive(5);
            setAutoReconnect(0);
        }
    }
}

void MQTTClient::_addQueue(QueueType type, MQTTComponent *component, const String &topic, QosType qos, bool retain, const String &payload)
{
    __LDBG_printf("type=%u topic=%s", type, topic.c_str());

    if (MQTTClient::_isMessageSizeExceeded(topic.length() + payload.length(), topic.c_str())) {
        return;
    }

    _queue.emplace_back(type, component, topic, qos, retain, payload);
    _Timer(_queueTimer).add(MQTT_QUEUE_RETRY_DELAY, (MQTT_QUEUE_TIMEOUT / MQTT_QUEUE_RETRY_DELAY), [this](Event::CallbackTimerPtr timer) {
        _queueTimerCallback();
    });
}

void MQTTClient::_queueTimerCallback()
{
    // process queue
    while(_queue.size()) {
        int result = -1;
        auto queue = _queue.front();
        __LDBG_printf("queue=%u topic=%s space=%u", _queue.size(), queue.getTopic().c_str(), getClientSpace());
        switch(queue.getType()) {
            case QueueType::SUBSCRIBE:
                result = subscribeWithId(queue.getComponent(), queue.getTopic(), queue.getQos());
                break;
            case QueueType::UNSUBSCRIBE:
                result = unsubscribeWithId(queue.getComponent(), queue.getTopic());
                break;
            case QueueType::PUBLISH:
                result = publishWithId(queue.getTopic(), queue.getRetain(), queue.getPayload(), queue.getQos());
                break;
        }
        if (result == 0) {
            return; // failure, retry again later
        }
        _queue.erase(_queue.begin());
    }

    __LDBG_print("queue done");
    _queueTimer.remove();
}

void MQTTClient::_clearQueue()
{
    __LDBG_printf("queue=%p rebroadcast_timer=%u queue_timer=%u queue_size=%u conn=%s", _autoDiscoveryQueue.get(), (bool)_autoDiscoveryRebroadcast, (bool)_queueTimer, _queue.size(), _connection());
    _autoDiscoveryQueue.reset();
    _autoDiscoveryRebroadcast.remove();
    _queueTimer.remove();
    _queue.clear();
}

bool MQTTClient::_isMessageSizeExceeded(size_t len, const char *topic)
{
    if (len > MQTT_MAX_MESSAGE_SIZE) {
        Logger_error(F("MQTT maximum message size exceeded: %u/%u: %s"), len, MQTT_MAX_MESSAGE_SIZE, topic);
        return true;
    }
    return false;
}
