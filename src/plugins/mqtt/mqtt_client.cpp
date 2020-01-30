/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#include <ESPAsyncWebServer.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include "progmem_data.h"
#include "mqtt_client.h"
#include "logger.h"
#include "plugins.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(Anonymous, "Anonymous");

MQTTClient *MQTTClient::_mqttClient = nullptr;

void MQTTClient::setupInstance()
{
    deleteInstance();
    if (config._H_GET(Config().flags).mqttMode != MQTT_MODE_DISABLED) {
        _mqttClient = _debug_new MQTTClient();
    }
}

void MQTTClient::deleteInstance()
{
    if (_mqttClient) {
        delete _mqttClient;
        _mqttClient = nullptr;
    }
}

MQTTClient::MQTTClient()
{
    _debug_printf_P(PSTR("MQTTClient::MQTTClient()\n"));

     _client = nullptr;
    _useNodeId = false;

    _setupClient();
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::ANY, MQTTClient::handleWiFiEvents);

    if (WiFi.isConnected()) {
        connect();
    }
}

MQTTClient::~MQTTClient()
{
    _debug_printf_P(PSTR("MQTTClient::~MQTTClient()\n"));
    WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, MQTTClient::handleWiFiEvents);
    _clearQueue();
    _timer.remove();

    _autoReconnectTimeout = 0;
    if (isConnected()) {
        disconnect(true);
    }
    delete _client;
}

void MQTTClient::_setupClient()
{
    _clearQueue();

    _client = _debug_new AsyncMqttClient();

    _maxMessageSize = MAX_MESSAGE_SIZE;
    _autoReconnectTimeout = DEFAULT_RECONNECT_TIMEOUT;

    auto host = config._H_STR(Config().mqtt_host);
    auto port = config._H_GET(Config().mqtt_port);
    IPAddress ip;
    if (ip.fromString(host)) {
        _client->setServer(ip, port);
    }
    else {
        _client->setServer(host, port);
    }
#if ASYNC_TCP_SSL_ENABLED
    auto flags = config._H_GET(Config().flags);
    if (flags.mqttMode == MQTT_MODE_SECURE) {
        _client->setSecure(true);
        auto fingerPrint = config._H_STR(Config().mqtt_fingerprint);
        if (*fingerPrint) {
            _client->addServerFingerprint(reinterpret_cast<const uint8_t *>(fingerPrint)); // addServerFingerprint supports multiple fingerprints
        }
    }
#endif
    _client->setCredentials(config._H_STR(Config().mqtt_username), config._H_STR(Config().mqtt_password));
    _client->setKeepAlive(config._H_GET(Config().mqtt_keepalive));

    _client->onConnect([this](bool sessionPresent) {
        this->onConnect(sessionPresent);
    });
    _client->onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
        this->onDisconnect(reason);
    });
    _client->onMessage([this](char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
        this->onMessageRaw(topic, payload, properties, len, index, total);
    });
#if MQTT_USE_PACKET_CALLBACKS
    _client->onPublish([this](uint16_t packetId) {
        this->onPublish(packetId);
    });
    _client->onSubscribe([this](uint16_t packetId, uint8_t qos) {
        this->onSubscribe(packetId, qos);
    });
    _client->onUnsubscribe([this](uint16_t packetId) {
        this->onUnsubscribe(packetId);
    });
#endif
}

void MQTTClient::registerComponent(MQTTComponentPtr component)
{
    _debug_printf_P(PSTR("MQTTClient::registerComponent(%p)\n"), component);
    unregisterComponent(component);
    _components.emplace_back(component);
    uint8_t num = 0;
    for(auto component: _components) {
        component->setNumber(num);
        num += component->getAutoDiscoveryCount();
    }
    _debug_printf_P(PSTR("MQTTClient::registerComponent() count %d\n"), _components.size());
}

void MQTTClient::unregisterComponent(MQTTComponentPtr component)
{
    _debug_printf_P(PSTR("MQTTClient::unregisterComponent(%p): components %u\n"), component, _components.size());
    if (_components.size()) {
        remove(component);
        _components.erase(std::remove(_components.begin(), _components.end(), component), _components.end());
    }
}

bool MQTTClient::hasMultipleComponments() const
{
    uint8_t count = 0;
    for(auto &&component: _components) {
        count += component->getAutoDiscoveryCount();
        if (count > 1) {
            return true;
        }
    }
    return false;
}

const String MQTTClient::getComponentName(uint8_t num)
{
    String deviceName = config._H_STR(Config().device_name);
    auto mqttClient = getClient();

    if (num != 0xff && mqttClient && mqttClient->hasMultipleComponments()) {
        deviceName += mqttClient->useNodeId() ? '/' : '_';
        deviceName += String(num);
    }
    // _debug_printf_P(PSTR("MQTTClient::getComponentName(): number=%u,client=%p,multiple=%u,device=%s\n"), num, mqttClient, mqttClient->hasMultipleComponments(), deviceName.c_str());
    return deviceName;
}

String MQTTClient::formatTopic(uint8_t num, const __FlashStringHelper *format, ...)
{
    PrintString topic = config._H_STR(Config().mqtt_topic);
    va_list arg;

    va_start(arg, format);
    topic.vprintf_P(RFPSTR(format), arg);
    va_end(arg);
    topic.replace(F("${device_name}"), getComponentName(num));
    // _debug_printf_P(PSTR("MQTTClient::formatTopic(): number=%u,topic=%s\n"), num, topic.c_str());
    return topic;
}

// String MQTTClient::formatTopic(uint8_t num, const char *format, ...) {
//     PrintString topic = config._H_STR(Config().mqtt_topic);
//     va_list arg;

//     va_start(arg, format);
//     topic.vprintf(format, arg);
//     va_end(arg);
//     topic.replace(F("${device_name}"), getComponentName(num));
//     return topic;
// }

bool MQTTClient::isConnected() const
{
    return _client->connected();
}

void MQTTClient::setAutoReconnect(uint32_t timeout)
{
    _debug_printf_P(PSTR("MQTTClient::setAutoReconnect(%d)\n"), timeout);
    _autoReconnectTimeout = timeout;
}

void MQTTClient::connect()
{
    _debug_printf_P(PSTR("MQTTClient::connect(): status %s connected %u\n"), connectionStatusString().c_str(), _client->connected());

    // remove reconnect timer if running and force disconnect
    auto timeout = _autoReconnectTimeout;
    _timer.remove();

    if (isConnected()) {
        _autoReconnectTimeout = 0; // disable auto reconnect
        disconnect(true);
        _autoReconnectTimeout = timeout;
    }

    _client->setCleanSession(true);

    // set last will to mark component as offline on disconnect
    // last will topic and payload need to be static
    static const char *lastWillPayload = "0";
    _lastWillTopic = formatTopic(-1, FSPGM(mqtt_status_topic));
    _debug_printf_P(PSTR("MQTTClient::setWill(%s) = %s\n"), _lastWillTopic.c_str(), lastWillPayload);
    _client->setWill(_lastWillTopic.c_str(), getDefaultQos(), 1, lastWillPayload, strlen(lastWillPayload));

    _client->connect();
//    autoReconnect(_autoReconnectTimeout);
}

void MQTTClient::disconnect(bool forceDisconnect)
{
    _debug_printf_P(PSTR("MQTTClient::disconnect(%d): status %s connected %u\n"), forceDisconnect, connectionStatusString().c_str(), _client->connected());
    if (!forceDisconnect) {
        // set offline
        publish(_lastWillTopic, getDefaultQos(), 1, FSPGM(0));
    }
    _client->disconnect(forceDisconnect);
}

void MQTTClient::autoReconnect(uint32_t timeout)
{
    _debug_printf_P(PSTR("MQTTClient::autoReconnect(%u) start\n"), timeout);
    if (timeout) {
        _timer.add(timeout, false, [this](EventScheduler::TimerPtr timer) {
            _debug_printf_P(PSTR("MQTTClient::autoReconnectTimer() timer = isConnected() %d\n"), this->isConnected());
            if (!this->isConnected()) {
                this->connect();
            }
        });

        // increase timeout on each reconnect attempt
        setAutoReconnect(timeout + timeout * 0.3);
    }
}

void MQTTClient::onConnect(bool sessionPresent)
{
    _debug_printf_P(PSTR("MQTTClient::onConnect(%d)\n"), sessionPresent);
    Logger_notice(F("Connected to MQTT server %s"), connectionDetailsString().c_str());

    _clearQueue();

    // set online
    publish(_lastWillTopic, getDefaultQos(), 1, FSPGM(1));

    // reset reconnect timer if connection was successful
    setAutoReconnect(DEFAULT_RECONNECT_TIMEOUT);

    for(auto component: _components) {
        component->onConnect(this);
#if MQTT_AUTO_DISCOVERY
        component->publishAutoDiscovery(this);
#endif
    }
}

void MQTTClient::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    _debug_printf_P(PSTR("MQTTClient::onDisconnect(%d): auto reconnect %d\n"), (int)reason, _autoReconnectTimeout);
    PrintString str;
    if (_autoReconnectTimeout) {
        str.printf_P(PSTR(", reconnecting in %d ms"), _autoReconnectTimeout);
    }
    Logger_notice(F("Disconnected from MQTT server %s, reason: %s%s"), connectionDetailsString().c_str(), _reasonToString(reason).c_str(), str.c_str());
    for(auto &&component: _components) {
        component->onDisconnect(this, reason);
    }
    _topics.clear();
    _clearQueue();

    // reconnect automatically
    autoReconnect(_autoReconnectTimeout);
}

void MQTTClient::subscribe(MQTTComponentPtr component, const String &topic, uint8_t qos)
{
    if (subscribeWithId(component, topic, qos) == 0) {
        _addQueue(MQTTQueue(QUEUE_SUBSCRIBE, component, topic, qos));
    }
}

int MQTTClient::subscribeWithId(MQTTComponentPtr component, const String &topic, uint8_t qos)
{
    _debug_printf_P(PSTR("MQTTClient::subscribe(%p, %s, qos %u)\n"), component, topic.c_str(), qos);
    auto result = _client->subscribe(topic.c_str(), qos);
    if (result && component) {
        _topics.emplace_back(topic, component);
    }
    return result;
}

void MQTTClient::unsubscribe(MQTTComponentPtr component, const String &topic)
{
    if (unsubscribeWithId(component, topic) == 0) {
        _addQueue(MQTTQueue(QUEUE_UNSUBSCRIBE, component, topic));
    }
}

int MQTTClient::unsubscribeWithId(MQTTComponentPtr component, const String &topic)
{
    int result = -1;
    _debug_printf_P(PSTR("MQTTClient::unsubscribe(%p, %s): topic in use %d\n"), component, topic.c_str(), _topicInUse(component, topic));
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
    _debug_printf_P(PSTR("MQTTClient::remove(%p): topics %u\n"), component, _topics.size());
    _topics.erase(std::remove_if(_topics.begin(), _topics.end(), [this, component](const MQTTTopic &mqttTopic) {
        if (mqttTopic.getComponent() == component) {
            _debug_printf_P(PSTR("MQTTClient::remove(): topic %s in use %d\n"), mqttTopic.getTopic().c_str(), _topicInUse(component, mqttTopic.getTopic()));
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
        _addQueue({QUEUE_PUBLISH, nullptr, topic, qos, retain, payload});
    }
}

int MQTTClient::publishWithId(const String &topic, uint8_t qos, bool retain, const String &payload)
{
    _debug_printf_P(PSTR("MQTTClient::publish(%s, qos %u, retain %d, %s)\n"), topic.c_str(), qos, retain, payload.c_str());
    return _client->publish(topic.c_str(), qos, retain, payload.c_str(), payload.length());
}

void MQTTClient::onMessageRaw(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    _debug_printf_P(PSTR("MQTTClient::onMessageRaw(%s, %s, idx:len:total %d:%d:%d, qos %u, dup %d, retain %d)\n"), topic, printable_string((const uint8_t *)payload, std::min(32, (int)len)).c_str(), index, len, total, properties.qos, properties.dup, properties.retain);
    if (total > _maxMessageSize) {
        _debug_printf(PSTR("MQTTClient::onMessageRaw discarding message, size exceeded %d/%d\n"), (unsigned)total, _maxMessageSize);
        return;
    }
    if (index == 0) {
        if (_buffer.length()) {
            _debug_printf(PSTR("MQTTClient::onMessageRaw discarding previous message, length %u\n"), _buffer.length());
            _buffer.clear();
        }
        _buffer.write(payload, len);
    } else if (index > 0) {
        if (index != _buffer.length()) {
            _debug_printf(PSTR("MQTTClient::onMessageRaw discarding message, invalid index %u buffer length %u\n"), index, _buffer.length());
            return;
        }
        _buffer.write(payload, len);
    }
    if (_buffer.length() == total) {
        _buffer.write(0);
        onMessage(topic, _buffer.getChar(), properties, total);
        _buffer.clear();
    }
}

void MQTTClient::onMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len)
{
    _debug_printf_P(PSTR("MQTTClient::onMessage(%s, %s, len %d, qos %u, dup %d, retain %d)\n"), topic, printable_string((const uint8_t *)payload, std::min(200, (int)len)).c_str(), len, properties.qos, properties.dup, properties.retain);
    for(auto iterator = _topics.begin(); iterator != _topics.end(); ++iterator) {
        if (_isTopicMatch(topic, iterator->getTopic().c_str())) {
            iterator->getComponent()->onMessage(this, topic, payload, len);
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

const String MQTTClient::_reasonToString(AsyncMqttClientDisconnectReason reason) const
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

const String MQTTClient::connectionDetailsString()
{
    String message;
    auto username = config._H_STR(Config().mqtt_username);
    if (*username) {
        message = username;
    } else {
        message = FSPGM(Anonymous);
    }
    message += '@';
    message += config._H_STR(Config().mqtt_host);
    message += ':';
    message += String(config._H_GET(Config().mqtt_port));
#if ASYNC_TCP_SSL_ENABLED
    auto flags = config._H_GET(Config().flags);
    if (flags.mqttMode == MQTT_MODE_SECURE) {
        message += F(", Secure MQTT");
    }
#endif
    return message;
}

const String MQTTClient::connectionStatusString()
{
    String message = connectionDetailsString();
    if (getClient() && getClient()->isConnected()) {
        message += F(", connected, ");
    } else {
        message += F(", disconnected, ");
    }
    message += F("topic ");

    message += formatTopic(-1, FSPGM(empty));
#if MQTT_AUTO_DISCOVERY
    if (config._H_GET(Config().flags).mqttAutoDiscoveryEnabled) {
        message += F(", discovery prefix '");
        message += config._H_STR(Config().mqtt_discovery_prefix);
        message += '\'';
    }
#endif
    return message;
}

void MQTTClient::getStatus(Print &out)
{
    if (getClient()) {
        out.print(connectionStatusString());
        out.print(F(HTML_S(br)));
        // auto &lwt = mqtt_session->getWill();
        // out.printf_P(PSTR("Last will: %s, qos %d, retain %s, available %s, not available %s" HTML_S(br) HTML_S(br) "Subscribed to:" HTML_S(br)), lwt.topic.c_str(), lwt.qos, lwt.retain ? "yes" : "no", lwt.available.c_str(), lwt.not_available.c_str());
        // for(auto &topic: mqtt_session->getTopics()) {
        //     out.print(topic.topic);
        //     out.printf_P(PSTR("<%u>"), topic.qos & ~0xc0);
        //     out.print(topic.qos & 0x080 ? F(" waiting for ack") : topic.qos & 0x040 ? F(" removal pending") : F(" acknowledged"));
        //     out.print(F(HTML_S(br)));
        // }
        // out.printf_P(PSTR(HTML_S(br) "Published/Acknowledged: %d/%d " HTML_S(br) "Received: %d " HTML_S(br)), mqtt_session->getPublished(), mqtt_session->getPublishedAck(), mqtt_session->getReceived());
    }
    else {
        out.print(FSPGM(Disabled));
    }
}

void MQTTClient::handleWiFiEvents(uint8_t event, void *payload)
{
    auto client = getClient();
    _debug_printf_P(PSTR("MQTTClient::handleWiFiEvents(%d, %p): client %p connected %d\n"), event, payload, client, client ? client->isConnected() : false);
    if (client) {
        if (event == WiFiCallbacks::EventEnum_t::CONNECTED) {
            if (!client->isConnected()) {
                client->setAutoReconnect(MQTTClient::DEFAULT_RECONNECT_TIMEOUT);
                client->connect();
            }
        } else if (event == WiFiCallbacks::EventEnum_t::DISCONNECTED) {
            if (client->isConnected()) {
                client->setAutoReconnect(0);
                client->disconnect(true);
            }
        }
    }
}

void MQTTClient::_addQueue(MQTTQueue &&queue)
{
    // _debug_printf_P(PSTR("MQTTClient::_addQueue(type %u): topic=%s\n"), queue.getType(), queue.getTopic().c_str());
    _queue.emplace_back(queue);

    if (_queueTimer.active()) {
        _queueTimer->setCallCounter(0); // reset retry counter for each new queue entry
    } else {
        _queueTimer.add(250, 20, MQTTClient::queueTimerCallback); // retry 20 times x 0.25s = 5s
    }
}

void MQTTClient::_queueTimerCallback(EventScheduler::TimerPtr timer)
{
    // _debug_printf_P(PSTR("MQTTClient::_queueTimerCallback(): attempt %u\n"), timer->getCallCounter());

    _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [this](const MQTTQueue &queue) {
        switch(queue.getType()) {
            case QUEUE_SUBSCRIBE:
                if (subscribeWithId(queue.getComponent(), queue.getTopic(), queue.getQos()) != 0) {
                    return true;
                }
                break;
            case QUEUE_UNSUBSCRIBE:
                if (unsubscribeWithId(queue.getComponent(), queue.getTopic()) != 0) {
                    return true;
                }
                break;
            case QUEUE_PUBLISH:
                if (publishWithId(queue.getTopic(), queue.getQos(), queue.getRetain(), queue.getPayload()) != 0) {
                    return true;
                }
                break;
        }
        return false;
    }), _queue.end());

    if (!_queue.size()) { // are we done?
        _queueTimer.remove();
    }
}

void MQTTClient::queueTimerCallback(EventScheduler::TimerPtr timer)
{
    getClient()->_queueTimerCallback(timer);
}

void MQTTClient::_clearQueue()
{
    _queueTimer.remove();
    _queue.clear();
}


class MQTTPlugin : public PluginComponent {
public:
    MQTTPlugin() {
        REGISTER_PLUGIN(this, "MQTTPlugin");
    }
    virtual PGM_P getName() const {
        return PSTR("mqtt");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override;
    virtual bool autoSetupAfterDeepSleep() const override;
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

static MQTTPlugin plugin;

MQTTPlugin::PluginPriorityEnum_t MQTTPlugin::getSetupPriority() const {
    return (PluginPriorityEnum_t)20;
}

bool MQTTPlugin::autoSetupAfterDeepSleep() const {
    return true;
}

void MQTTPlugin::setup(PluginSetupMode_t mode)
{
    MQTTClient::setupInstance();
}

void MQTTPlugin::reconfigure(PGM_P source)
{
    MQTTClient::deleteInstance();
    if (config._H_GET(Config().flags).mqttMode != MQTT_MODE_DISABLED) {
        MQTTClient::setupInstance();
    }
}

void MQTTPlugin::restart()
{
    // crashing sometimes
    // MQTTClient::deleteInstance();
}

void MQTTPlugin::getStatus(Print &output)
{
    MQTTClient::getStatus(output);
}

void MQTTPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

    //TODO save form doesnt work

    form.add<uint8_t>(F("mqtt_enabled"), _H_STRUCT_FORMVALUE(Config().flags, uint8_t, mqttMode));
    form.addValidator(new FormRangeValidator(MQTT_MODE_DISABLED, MQTT_MODE_SECURE));

    form.add<sizeof Config().mqtt_host>(F("mqtt_host"), config._H_W_STR(Config().mqtt_host));
    form.addValidator(new FormValidHostOrIpValidator());

    form.add<uint16_t>(F("mqtt_port"), &config._H_W_GET(Config().mqtt_port));
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
    form.addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

    form.add<sizeof Config().mqtt_username>(F("mqtt_username"), config._H_W_STR(Config().mqtt_username));
    form.addValidator(new FormLengthValidator(0, sizeof(Config().mqtt_username) - 1));

    form.add<sizeof Config().mqtt_password>(F("mqtt_password"), config._H_W_STR(Config().mqtt_password));
    // form.addValidator(new FormLengthValidator(8, sizeof(Config().mqtt_password) - 1));

    form.add<sizeof Config().mqtt_topic>(F("mqtt_topic"), config._H_W_STR(Config().mqtt_topic));
    form.addValidator(new FormLengthValidator(3, sizeof(Config().mqtt_topic) - 1));

    form.add<uint8_t>(F("mqtt_qos"), &config._H_W_GET(Config().mqtt_qos));
    form.addValidator(new FormRangeValidator(F("Invalid value for QoS"), 0, 2));

#if MQTT_AUTO_DISCOVERY
    form.add<bool>(F("mqtt_auto_discovery"), _H_STRUCT_FORMVALUE(Config().flags, bool, mqttAutoDiscoveryEnabled));

    form.add<sizeof Config().mqtt_discovery_prefix>(F("mqtt_discovery_prefix"), config._H_W_STR(Config().mqtt_discovery_prefix));
    form.addValidator(new FormLengthValidator(0, sizeof(Config().mqtt_discovery_prefix) - 1));
#endif

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<connect|disconnect|force-disconnect|secure|unsecure|disable>", "Connect or disconnect from server", "Display MQTT status");
#if MQTT_AUTO_DISCOVERY_CLIENT

#include "mqtt_auto_discovery_client.h"
#include <JsonCallbackReader.h>
#include <JsonVar.h>
#include <BufferStream.h>

#if LOGGER
#include "logger_stream.h"
#endif

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MQTTAD, "MQTTAD", "<1=enable|0=disable>", "Enable MQTT auto discovery");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MQTTLAD, "MQTTLAD", "List MQTT auto discovery");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MQTTPAD, "MQTTPAD", "[<id>][,<parsed|raw>]", "Print auto discovery payload");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MQTTDAD, "MQTTDAD", "<id>[,<id>,...]", "Delete MQTT auto discovery");
#endif

#if MQTT_AUTO_DISCOVERY_CLIENT

static bool mqtt_at_mode_is_connected(AtModeArgs &args)
{
    if (!MQTTClient::getClient() || !MQTTClient::getClient()->isConnected()) {
        args.print(F("Client not connected"));
        return false;
    }
    return true;
}

static bool mqtt_at_mode_auto_discovery(AtModeArgs &args)
{
    if (!MQTTAutoDiscovery::isEnabled()) {
        args.print(F("Auto discovery disabled"));
        return false;
    }
    return true;
}

static bool mqtt_at_mode_auto_discovery_client(AtModeArgs &args)
{
    if (!MQTTAutoDiscoveryClient::getInstance()) {
        args.print(F("Auto discovery client not running"));
        return false;
    }
    return true;
}

#endif

void MQTTPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTT), getName());
#if MQTT_AUTO_DISCOVERY_CLIENT
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTTAD), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTTLAD), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTTPAD), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTTDAD), getName());
#endif
}

bool MQTTPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        auto &serial = args.getStream();
        if (args.isQueryMode()) {
            if (MQTTClient::getClient()) {
                args.print();
                serial.print(F("status: "));
                serial.println(MQTTClient::connectionStatusString());
            } else {
                serial.println(FSPGM(disabled));
            }
        } else if (MQTTClient::getClient() && args.requireArgs(1, 1)) {
            auto &client = *MQTTClient::getClient();
            args.print();
            if (args.isAnyMatchIgnoreCase(0, F("connect,con"))) {
                serial.print(F("connect: "));
                serial.println(MQTTClient::connectionStatusString());
                client.setAutoReconnect(MQTTClient::DEFAULT_RECONNECT_TIMEOUT);
                client.connect();
            }
            else if (args.isAnyMatchIgnoreCase(0, F("disconnect,dis"))) {
                serial.println(F("disconnect"));
                client.disconnect(false);
            }
            else if (args.equalsIgnoreCase(0, F("force-disconnect,fdis"))) {
                serial.println(F("force disconnect"));
                client.setAutoReconnect(0);
                client.disconnect(true);
            }
            else if (args.isTrue(0)) {
                config.discard();
                auto &flags = config._H_W_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_UNSECURE;
                config.write();
                args.printf_P(PSTR("MQTT unsecure %s"), FSPGM(enabled));
            }
            else if (args.isAnyMatchIgnoreCase(0, F("secure"))) {
                config.discard();
                auto &flags = config._H_W_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_SECURE;
                config.write();
                args.printf_P(PSTR("MQTT secure %s"), FSPGM(enabled));
            }
            else if (args.isFalse(0)) {
                config.discard();
                auto &flags = config._H_W_GET(Config().flags);
                flags.mqttMode = MQTT_MODE_DISABLED;
                config.write();
                client.setAutoReconnect(0);
                client.disconnect(true);
                args.printf_P(PSTR("MQTT %s"), FSPGM(disabled));
            }
        }
        return true;
    }
#if MQTT_AUTO_DISCOVERY_CLIENT
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTTAD))) {
        if (args.requireArgs(1, 1) && mqtt_at_mode_is_connected(args) && mqtt_at_mode_auto_discovery(args)) {
            int state = args.toInt(0);
            if (state == 0 && MQTTAutoDiscoveryClient::getInstance()) {
                MQTTAutoDiscoveryClient::deleteInstance();
                args.ok();
            }
            else if (state && !MQTTAutoDiscoveryClient::getInstance()) {
                args.ok();
#if LOGGER && 0
                MQTTAutoDiscoveryClient::createInstance(new LoggerStream()); // we don't care about that memory leak
#else
                MQTTAutoDiscoveryClient::createInstance(&serial);
#endif
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTTLAD))) {
         if (mqtt_at_mode_auto_discovery(args) && mqtt_at_mode_auto_discovery_client(args)) {
             for(const auto &devicePtr: MQTTAutoDiscoveryClient::getInstance()->getDiscovery()) {
                 const auto &device = *devicePtr;
                 args.printf_P(PSTR("Id=%03u, name='%s' topic='%s' payload=%u model='%*.*s' sw_version='%*.*s' manufacturer='%*.*s'"),
                    device.id,
                    device.name.c_str(),
                    device.topic.c_str(),
                    device.payload.length(),
                    device.model.length(), device.model.length(), device.model.getData(),
                    device.swVersion.length(), device.swVersion.length(), device.swVersion.getData(),
                    device.manufacturer.length(), device.manufacturer.length(), device.manufacturer.getData()
                );
             }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTTPAD))) {
        if (mqtt_at_mode_auto_discovery(args) && mqtt_at_mode_auto_discovery_client(args)) {
            uint16_t id = args.toInt(0);
            auto formatRaw = args.equalsIgnoreCase(1, F("raw"));
            bool found = false;

            for(const auto &devicePtr: MQTTAutoDiscoveryClient::getInstance()->getDiscovery()) {
                if (devicePtr->id == id || id == 0) {
                    auto &serial = args.getStream();
                    const auto &device = *devicePtr;
                    found = true;
                    args.printf_P(PSTR("%s (%u):"), device.name.c_str(), device.id);

                    if (id == 0) {
                        repeat(78, serial.write(repeat_last_iteration ? '\n' : '-'));
                    }
                    if (formatRaw) {
                        serial.println(device.payload);
                    }
                    else {
                        BufferStream payload;
                        payload.write(reinterpret_cast<const uint8_t *>(device.payload.c_str()), device.payload.length());
                        JsonCallbackReader reader(payload, [&serial](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
                            serial.printf_P(PSTR("%s = %s\n"), json.getPath().c_str(), JsonVar::formatValue(value, json.getType()).c_str());
                            return true;
                        });
                        reader.parse();
                    }
                    if (id == 0) {
                        repeat(78, serial.write(repeat_last_iteration ? '\n' : '-'));
                    }
                }
            }
            if (!found) {
                if (id) {
                    args.printf_P(PSTR("Could not find id %u"), id);
                }
                else {
                    args.printf_P(PSTR("+MQTTPAD: No devices found"));
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTTDAD))) {
        if (args.requireArgs(1) && mqtt_at_mode_auto_discovery(args) && mqtt_at_mode_auto_discovery_client(args)) {
            std::vector<uint16_t> ids;
            ids.reserve(args.size());
            for(auto arg: args.getArgs()) {
                ids.push_back(atoi(*arg));
            }
            for(const auto &devicePtr: MQTTAutoDiscoveryClient::getInstance()->getDiscovery()) {
                auto iterator = std::find(ids.begin(), ids.end(), devicePtr->id);
                if (iterator != ids.end()) {
                    args.printf_P(PSTR("Sending empty payload to %s"), devicePtr->topic.c_str());
                    MQTTClient::getClient()->publish(devicePtr->topic, 2, true, _sharedEmptyString);
                    ids.erase(iterator);
                    if (!ids.size()) {
                        break;
                    }
                }
            }
            if (ids.size()) {
                auto &serial = args.getStream();
                serial.printf_P(PSTR("+MQTTDAD: Could not find: "));
                for(auto id: ids) {
                    serial.print(id);
                    serial.print(' ');
                }
                serial.println();
            }
         }
         return true;
    }
#endif
    return false;
}

#endif

#endif
