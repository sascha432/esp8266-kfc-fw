/**
 * Author: sascha_lammers@gmx.de
 */

// Home Assistant auto discovery payloads

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
#include "mqtt_strings.h"
#include "auto_discovery.h"
#include "mqtt_client.h"
#include "templates.h"
#include "JsonTools.h"
#include <debug_helper.h>

#if DEBUG_MQTT_CLIENT
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
#    define MQTT_DEVICE_REG_CONNECTIONS  "cns"
#    define MQTT_DEVICE_REG_IDENTIFIERS  "ids"
#    define MQTT_DEVICE_REG_NAME         "name"
#    define MQTT_DEVICE_REG_MANUFACTURER "mf"
#    define MQTT_DEVICE_REG_MODEL        "mdl"
#    define MQTT_DEVICE_REG_SW_VERSION   "sw"
#else
#    define MQTT_DEVICE_REG_CONNECTIONS  "connections"
#    define MQTT_DEVICE_REG_IDENTIFIERS  "identifiers"
#    define MQTT_DEVICE_REG_NAME         "name"
#    define MQTT_DEVICE_REG_MANUFACTURER "manufacturer"
#    define MQTT_DEVICE_REG_MODEL        "model"
#    define MQTT_DEVICE_REG_SW_VERSION   "sw_version"
#endif

using KFCConfigurationClasses::System;
using namespace MQTT::AutoDiscovery;

Entity::Entity() :
    _format(FormatType::JSON),
    _type(ComponentType::SWITCH),
    _hasName(false)
{
}

bool Entity::create(ComponentPtr component, const String &componentName, FormatType format)
{
    return create(component->getType(), componentName, format);
}

bool Entity::create(ComponentType componentType, const String &componentName, FormatType format)
{
    String suffix = System::Device::getObjectIdOrName();
    if (componentName.length()) {
        if (!StrView(componentName).startsWith('/')) {
            suffix += '/';
        }
        suffix += componentName;
    }
    return _create(componentType, suffix, format);
}

bool Entity::_create(ComponentType componentType, const String &name, FormatType format)
{
    _format = format;
    _type = componentType;
    _topic = MqttClient::getAutoDiscoveryPrefix();
    _topic += '/';
    _topic += Component::getNameByType(componentType);
    if (!StrView(name).startsWith('/')) {
        _topic += '/';
    }
    _topic += name;
    if (!StrView(_topic).endsWith('/')) {
        _topic += '/';
    }
    _topic += F("config");

    if (format == FormatType::TOPIC) {
        return false;
    }

    _discovery = PrintString();
    _discovery += '{';
    #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
        _baseTopic = Client::getBaseTopic();
        addParameter(F("~"), _baseTopic);
    #endif

    PrintString uniqueId;
    WebTemplate::printUniqueId(uniqueId, name);
    if (componentType != ComponentType::DEVICE_AUTOMATION) {
        // the name is added by finalize() if the component does not add one
        _name = name;
        addParameter(FSPGM(mqtt_platform), FSPGM(mqtt));
        addParameter(FSPGM(mqtt_unique_id), uniqueId);
        addParameter(FSPGM(mqtt_availability_topic), Client::formatTopic(MQTT_AVAILABILITY_TOPIC));
        #if MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF
            addParameter(FSPGM(mqtt_payload_available), MQTT_AVAILABILITY_TOPIC_ONLINE);
            addParameter(FSPGM(mqtt_payload_not_available), MQTT_AVAILABILITY_TOPIC_OFFLINE);
        #endif
    }

    PrintString model;
    WebTemplate::printModel(model);

    #define JSON_VALUE_START            "\":\""
    #define JSON_NEXT_KEY_START         "\",\""

    auto namePtr = System::Device::getTitle();
    if (StrView(namePtr) == FSPGM(KFC_Firmware)) {
        namePtr = System::Device::getName();
    }

    _discovery.print(F("\"device\":{\"" MQTT_DEVICE_REG_IDENTIFIERS "\":[\""));
    _discovery.print(uniqueId);
    _discovery.print(F("\"],\"" MQTT_DEVICE_REG_CONNECTIONS "\":[[\"mac" JSON_NEXT_KEY_START));
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    printMacAddress(mac, _discovery);
    _discovery.print(F("\"]],\"" MQTT_DEVICE_REG_MODEL JSON_VALUE_START));
    _discovery.print(model);
    _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_NAME JSON_VALUE_START));
    _discovery.print(namePtr);
    _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_SW_VERSION JSON_VALUE_START "KFC FW "));
    _discovery.print(KFCFWConfiguration::getFirmwareVersion());
    _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_MANUFACTURER JSON_VALUE_START));
    _discovery.print(FSPGM(KFCLabs));
    _discovery.print(F("\"},"));

    return true;
}

void Entity::addObjectId(const String &value)
{
    // Home Assistant 2026.4 removed "object_id": "default_entity_id" is the complete entity id
    // <component>.<lowercase letters, digits and underscores>
    String id = Component::getNameByType(_type);
    id += '.';
    auto start = id.length();
    for (auto ptr = value.c_str(); *ptr; ptr++) {
        auto ch = *ptr;
        if (isalnum(ch)) {
            id += static_cast<char>(tolower(ch));
        }
        else if (id.length() > start && id.charAt(id.length() - 1) != '_') {
            id += '_';
        }
    }
    while (id.length() > start && id.charAt(id.length() - 1) == '_') {
        id.remove(id.length() - 1);
    }
    addParameter(FSPGM(mqtt_default_entity_id), id);
}

void Entity::__addParameter(NameType name, const char *str, bool quotes)
{
    if (_format != FormatType::JSON) {
        return;
    }
    _discovery.printf_P(PSTR("\"%s\":"), name);
    if (quotes) {
        _discovery.print('"');
    }
    #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
        // replace the base topic of topics ("..._t") with "~"
        auto len = strlen_P(RFPSTR(name));
        if (len > 2 && pgm_read_word(RFPSTR(name) + len - 2) == (('_') | ('t' << 8))) {
            if (strncmp_P(_baseTopic.c_str(), str, _baseTopic.length()) == 0) {
                str += _baseTopic.length();
                _discovery.print('~');
            }
        }
    #endif
    if (quotes) {
        KFCJson::JsonTools::Utf8Buffer buffer;
        KFCJson::JsonTools::printToEscaped(_discovery, FPSTR(str), &buffer);
        _discovery.print(F("\","));
    }
    else {
        _discovery.print(FPSTR(str));
        _discovery.print(',');
    }
}

void Entity::finalize()
{
    if (_format != FormatType::JSON || !_discovery.length()) {
        return;
    }
    if (!_hasName && _name.length()) {
        addParameter(FSPGM(mqtt_name), _name);
    }
    _name = String();
    // replace the last comma
    _discovery.remove(_discovery.length() - 1);
    _discovery += '}';
}

String Entity::getConfigWildcardTopic()
{
    return PrintString(F("%s/+/%s/config"), MqttClient::getAutoDiscoveryPrefix(), System::Device::getObjectIdOrName());
}

String Entity::getConfig2ndLevelWildcardTopic()
{
    return PrintString(F("%s/+/%s/+/config"), MqttClient::getAutoDiscoveryPrefix(), System::Device::getObjectIdOrName());
}

String Entity::getTriggersTopic()
{
    return Client::formatTopic(F("/triggers"));
}
