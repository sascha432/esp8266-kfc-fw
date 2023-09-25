/**
 * Author: sascha_lammers@gmx.de
 */
#include <Arduino_compat.h>
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

StringVector MQTT::Client::_createAutoDiscoveryTopics() const
{
    StringVector wildcards;
    wildcards.emplace_back(std::move(AutoDiscovery::Entity::getConfigWildcardTopic()));
    wildcards.emplace_back(std::move(AutoDiscovery::Entity::getConfig2ndLevelWildcardTopic()));
    return wildcards;
}

String MQTT::Client::connectionDetailsString()
{
    auto message = PrintString(F("%s@%s:%u"), _username.length() ? _username.c_str() : SPGM(Anonymous), (IPAddress_isValid(_address) ? _address.toString().c_str() : _hostname.c_str()), _port);
    message.printf_P(PSTR(", QoS %u"), _getDefaultQos());
    #if ASYNC_TCP_SSL_ENABLED
        if (_config._get_enum_mode() == ModeType::SECURE) {
            message += F(", Secure MQTT");
        }
    #endif
    return message;
}

String MQTT::Client::connectionStatusString()
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

    #if MQTT_AUTO_DISCOVERY
        message += MQTT::Client::_getBaseTopic();
    #else
        message += formatTopic(emptyString);
    #endif

    #if MQTT_AUTO_DISCOVERY
        if (_config.auto_discovery) {
            message += F(HTML_S(br) "Auto discovery prefix '");
            message += Plugins::MqttClient::getAutoDiscoveryPrefix();
            message += '\'';
        }
    #endif
    return message;
}

MQTT::NameType MQTT::Client::_reasonToString(AsyncMqttClientDisconnectReason reason) const
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

int8_t MQTT::Client::toBool(const char *str, int8_t invalid)
{
    while(isspace(*str)) {
        str++;
    }
    if (!*str) {
        return invalid;
    }
    // check if it is an integer
    char *end = nullptr;
    auto value = strtol(str, &end, 0);
    while(isspace(*end)) {
        end++;
    }
    if (end && !*end) {
        return value != 0;
    }
    // check if it is a string match
    auto tmp = String(str);
    auto cTmp = tmp.trim().c_str();
    if (
        // (strcasecmp_P(cTmp, PSTR("true")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("on")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("yes")) == 0) ||
        // (strcasecmp_P(cTmp, PSTR("online")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("enable")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("enabled")) == 0) ||
        (strcasecmp_P(cTmp, SPGM(mqtt_bool_on)) == 0) ||
        (strcasecmp_P(cTmp, SPGM(mqtt_status_topic_online)) == 0) ||
        (strcasecmp_P(cTmp, SPGM(mqtt_bool_true)) == 0)
    ) {
        return true;
    }
    if (
        // (strcasecmp_P(cTmp, PSTR("false")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("off")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("no")) == 0) ||
        // (strcasecmp_P(cTmp, PSTR("offline")) == 0) ||
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
