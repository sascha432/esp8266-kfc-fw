/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include "mqtt_client.h"
#include "logger.h"
#include "plugins.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DEFINE_ENUM(MQTTQueueEnum_t);

PROGMEM_STRING_DEF(Anonymous, "Anonymous");

MQTTClient *MQTTClient::_mqttClient = nullptr;

void MQTTClient::setupInstance()
{
    _debug_println();
    deleteInstance();
    if (Config_MQTT::getMode() != MQTT_MODE_DISABLED) {
        _mqttClient = new MQTTClient();
    }
}

void MQTTClient::deleteInstance()
{
    _debug_println();
    if (_mqttClient) {
        delete _mqttClient;
        _mqttClient = nullptr;
    }
}

MQTTClient::MQTTClient() : _client(nullptr), _componentsEntityCount(0), _lastWillPayload('0')
{
    _debug_println();

    _host = Config_MQTT::getHost();
    _username = Config_MQTT::getUsername();
    _password = Config_MQTT::getPassword();
    _config = Config_MQTT::getConfig();

    _setupClient();
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, MQTTClient::handleWiFiEvents);

    if (WiFi.isConnected()) {
        connect();
    }
}

MQTTClient::~MQTTClient()
{
    _debug_println();
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

void MQTTClient::_setupClient()
{
    _debug_println();
    _clearQueue();

    _client = new AsyncMqttClient();

    _autoReconnectTimeout = MQTT_AUTO_RECONNECT_TIMEOUT;

    IPAddress ip;
    if (ip.fromString(_host)) {
        _client->setServer(ip, _config.port);
    }
    else {
        _client->setServer(_host.c_str(), _config.port);
    }
#if ASYNC_TCP_SSL_ENABLED
    if (Config_MQTT::getMode() == MQTT_MODE_SECURE) {
        _client->setSecure(true);
        auto fingerPrint = Config_MQTT::getFingerprint();
        if (*fingerPrint) {
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
        debug_printf_P(PSTR("component registered multiple times: name=%s component=%p\n"), component->getComponentName(), component);
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
    topic = Config_MQTT::getTopic();
    topic.replace(F("${device_name}"), config.getDeviceName());
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
        publish(_lastWillTopic, getDefaultQos(), true, String(0));
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

String MQTTClient::connectionDetailsString()
{
    auto message = PrintString(F("%s@%s:%u"), _username.length() ? _username.c_str() : SPGM(Anonymous), _host.c_str(), _config.port);
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
    if (config._H_GET(Config().flags).mqttAutoDiscoveryEnabled) {
        message += F(HTML_S(br) "Auto discovery prefix '");
        message += Config_MQTT::getDiscoveryPrefix();
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
    publish(_lastWillTopic, getDefaultQos(), true, String(1));

    // reset reconnect timer if connection was successful
    setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);

    for(const auto &component: _components) {
        component->onConnect(this);
    }
#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled() && !_components.empty()) {
        _autoDiscoveryQueue.reset(new MQTTAutoDiscoveryQueue(*this));
        _autoDiscoveryQueue->publish();
    }
#endif
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
    if (!_queue.empty() || subscribeWithId(component, topic, qos) == 0) {
        _addQueue(MQTTQueueType::SUBSCRIBE, component, topic, qos, 0, String());
    }
}

int MQTTClient::subscribeWithId(MQTTComponentPtr component, const String &topic, uint8_t qos)
{
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

void MQTTClient::publish(const String &topic, uint8_t qos, bool retain, const String &payload)
{
    if (publishWithId(topic, qos, retain, payload) == 0) {
        _addQueue(MQTTQueueType::PUBLISH, nullptr, topic, qos, retain, payload);
    }
}

int MQTTClient::publishWithId(const String &topic, uint8_t qos, bool retain, const String &payload)
{
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
                result = publishWithId(queue.getTopic(), queue.getQos(), queue.getRetain(), queue.getPayload());
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


class MQTTPlugin : public PluginComponent {
public:
    MQTTPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

static MQTTPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    MQTTPlugin,
    "mqtt",             // name
    "MQTT",             // friendly name
    "",                 // web_templates
    "mqtt",             // config_forms
    "network",          // reconfigure_dependencies
    PluginComponent::PriorityType::MQTT,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

MQTTPlugin::MQTTPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(MQTTPlugin))
{
    REGISTER_PLUGIN(this, "MQTTPlugin");
}

void MQTTPlugin::setup(SetupModeType mode)
{
    MQTTClient::setupInstance();
}

void MQTTPlugin::reconfigure(const String &source)
{
    _debug_printf_P(PSTR("source=%s\n"), source.c_str());
    MQTTClient::deleteInstance();
    if (config._H_GET(Config().flags).mqttMode != MQTT_MODE_DISABLED) {
        MQTTClient::setupInstance();
    }
}

void MQTTPlugin::shutdown()
{
    // crashing sometimes
    // MQTTClient::deleteInstance();
}

void MQTTPlugin::getStatus(Print &output)
{
    auto client = MQTTClient::getClient();
    if (client) {
        output.print(client->connectionStatusString());
        output.printf_P(PSTR(HTML_S(br) "%u components, %u entities" HTML_S(br)), client->_components.size(), client->_componentsEntityCount);
    }
    else {
        output.print(FSPGM(Disabled));
    }
}


void MQTTPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    form.add<uint8_t>(F("mqtt_enabled"), _H_FLAGS_VALUE(Config().flags, mqttMode));
    form.addValidator(new FormRangeValidator(MQTT_MODE_DISABLED, MQTT_MODE_SECURE));

    form.add(F("mqtt_host"), _H_STR_VALUE(Config().mqtt.host));
    form.addValidator(new FormValidHostOrIpValidator());

    form.add<uint16_t>(F("mqtt_port"), _H_STRUCT_VALUE(Config().mqtt.config, port));
    form.addValidator(new FormTCallbackValidator<uint16_t>([](uint16_t value, FormField &field) {
#if ASYNC_TCP_SSL_ENABLED
        if (value == 0 && static_cast<FormBitValue<ConfigFlags_t, 3> *>(field.getForm().getField(F("mqtt_enabled")))->getValue() == MQTT_MODE_SECURE) {
            value = 8883;
            field.setValue(String(value));
        } else
#endif
        if (value == 0) {
            value = 1883;
            field.setValue(String(value));
        }
        return true;
    }));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port), 1, 65535));

    form.add(F("mqtt_username"), _H_STR_VALUE(Config().mqtt.username));
    form.addValidator(new FormLengthValidator(0, sizeof(Config().mqtt.username) - 1));

    form.add(F("mqtt_password"), _H_STR_VALUE(Config().mqtt.password));
    // form.addValidator(new FormLengthValidator(8, sizeof(Config().mqtt_password) - 1));

    form.add(F("mqtt_topic"), _H_STR_VALUE(Config().mqtt.topic));
    form.addValidator(new FormLengthValidator(3, sizeof(Config().mqtt.topic) - 1));

    form.add<uint8_t>(F("mqtt_qos"), _H_STRUCT_VALUE(Config().mqtt.config, qos));
    form.addValidator(new FormRangeValidator(F("Invalid value for QoS"), 0, 2));

#if MQTT_AUTO_DISCOVERY
    form.add<bool>(F("mqtt_auto_discovery"), _H_FLAGS_BOOL_VALUE(Config().flags, mqttAutoDiscoveryEnabled));

    form.add(F("mqtt_discovery_prefix"), _H_STR_VALUE(Config().mqtt.discovery_prefix));
    form.addValidator(new FormLengthValidator(0, sizeof(Config().mqtt.discovery_prefix) - 1));
#endif

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<connect|disconnect|force-disconnect|secure|unsecure|disable>", "Connect or disconnect from server", "Display MQTT status");

void MQTTPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTT), name);
}

bool MQTTPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        auto client = MQTTClient::getClient();
        auto &serial = args.getStream();
        if (args.isQueryMode()) {
            if (client) {
                args.printf_P(PSTR("status: %s"), client->connectionStatusString().c_str());
            } else {
                serial.println(FSPGM(disabled));
            }
        } else if (client && args.requireArgs(1, 1)) {
            auto &client = *MQTTClient::getClient();
            if (args.isAnyMatchIgnoreCase(0, F("connect,con"))) {
                args.printf_P(PSTR("connect: %s"), client.connectionStatusString().c_str());
                client.setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);
                client.connect();
            }
            else if (args.isAnyMatchIgnoreCase(0, F("disconnect,dis"))) {
                args.print(F("disconnect"));
                client.disconnect(false);
            }
            else if (args.equalsIgnoreCase(0, F("force-disconnect,fdis"))) {
                args.print(F("force disconnect"));
                client.setAutoReconnect(0);
                client.disconnect(true);
            }
            else if (args.isTrue(0)) {
                auto flags = config._H_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_UNSECURE;
                config._H_SET(Config().flags, flags);
                config.write();
                args.printf_P(PSTR("MQTT unsecure %s"), FSPGM(enabled));
            }
            else if (args.isAnyMatchIgnoreCase(0, F("secure"))) {
                auto flags = config._H_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_SECURE;
                config._H_SET(Config().flags, flags);
                config.write();
                args.printf_P(PSTR("MQTT secure %s"), FSPGM(enabled));
            }
            else if (args.isFalse(0)) {
                auto flags = config._H_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_DISABLED;
                config._H_SET(Config().flags, flags);
                config.write();
                client.setAutoReconnect(0);
                client.disconnect(true);
                args.printf_P(PSTR("MQTT %s"), FSPGM(disabled));
            }
        }
        return true;
    }
    return false;
}

#endif
