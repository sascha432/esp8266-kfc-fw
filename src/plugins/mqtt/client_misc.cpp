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

using namespace MQTT;

StringVector MQTTClient::_createAutoDiscoveryTopics() const
{
    StringVector wildcards;
    wildcards.emplace_back(std::move(AutoDiscovery::Entity::getConfigWildcardTopic()));
    wildcards.emplace_back(std::move(AutoDiscovery::Entity::getConfig2ndLevelWildcardTopic()));
    return wildcards;
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

NameType MQTTClient::_reasonToString(AsyncMqttClientDisconnectReason reason) const
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

int8_t MQTTClient::toBool(const char *str, int8_t invalid)
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
        (strcasecmp_P(cTmp, PSTR("true")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("on")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("yes")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("online")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("enable")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("enabled")) == 0)
    ) {
        return true;
    }
    if (
        (strcasecmp_P(cTmp, PSTR("false")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("off")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("no")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("offline")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("disable")) == 0) ||
        (strcasecmp_P(cTmp, PSTR("disabled")) == 0)
    ) {
        return false;
    }
    return invalid;
}
