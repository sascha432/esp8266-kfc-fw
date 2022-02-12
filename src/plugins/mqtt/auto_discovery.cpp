/**
 * Author: sascha_lammers@gmx.de
 */

// Yaml code for Home Assistant and auto discovery

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

using KFCConfigurationClasses::System;
using namespace MQTT::AutoDiscovery;

bool Entity::create(ComponentPtr component, const String &componentName, FormatType format)
{
    return create(component->getType(), componentName, format);
}

bool Entity::create(ComponentType componentType, const String &componentName, FormatType format)
{
    String suffix = System::Device::getObjectIdOrName();
    if (componentName.length()) {
        if (!componentName.startsWith('/')) {
            suffix += '/';
        }
        suffix += componentName;
    }
    return _create(componentType, suffix, format);
}

String Entity::getWildcardTopic()
{
    return PrintString(F("%s/+/%s/#"), MqttClient::getAutoDiscoveryPrefix(), System::Device::getObjectIdOrName());
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
    return MQTT::Client::formatTopic(F("/triggers"));
}

bool Entity::_create(ComponentType componentType, const String &name, FormatType format, NameType platform)
{
    String uniqueId;

    _format = format;
    _topic = MqttClient::getAutoDiscoveryPrefix();
    _topic += '/';
    _topic += Component::getNameByType(componentType);
    if (!name.startsWith('/')) {
        _topic += '/';
    }
    _topic += name;

    if (!_topic.endsWith('/')) {
        _topic += '/';
    }
    _topic += F("config");

    _discovery = PrintString();
    if (format == FormatType::TOPIC) {
        __LDBG_printf("MQTT auto discovery topic only '%s'", _topic.c_str());
        return false;
    }

    if (_format == FormatType::JSON) {
        _discovery += '{';
        #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
            _baseTopic = MQTT::Client::getBaseTopic();
            __DBG_printf("base_topic=%s", __S(_baseTopic));
            addParameter(F("~"), _baseTopic);
        #endif
    }
    else {
        _discovery += Component::getNameByType(componentType);
        _discovery += F(":\n  - ");
    }
    uniqueId = _getUniqueId(name);
    if (componentType != ComponentType::DEVICE_AUTOMATION) {
        addParameter(FSPGM(name), name);
        addParameter(F("platform"), platform);
        if (format == FormatType::JSON) {
            addParameter(FSPGM(mqtt_unique_id), uniqueId);
        }
        addParameter(FSPGM(mqtt_availability_topic), MQTT::Client::formatTopic(MQTT_AVAILABILITY_TOPIC));
        #if MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF
            addParameter(FSPGM(mqtt_payload_available), MQTT_AVAILABILITY_TOPIC_ONLINE);
            addParameter(FSPGM(mqtt_payload_not_available), MQTT_AVAILABILITY_TOPIC_OFFLINE);
        #endif
    }
    if (_format == FormatType::JSON) {
        PrintString model;
        WebTemplate::printModel(model);

        #define JSON_VALUE_START            "\":\""
        #define JSON_NEXT_KEY_START         "\",\""

        auto namePtr = System::Device::getTitle();
        if (strcmp_P(namePtr, SPGM(KFC_Firmware)) == 0) {
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
    }

    __LDBG_printf("MQTT auto discovery topic '%s'", _topic.c_str());
    return true;
}

void Entity::__addParameter(NameType name, const char *str, bool quotes)
{
    __DBG_validatePointer(name, VP_HPS);
    __DBG_validatePointer(str, VP_HPS);
    if (_format == FormatType::TOPIC) {
        return;
    }
    else if (_format == FormatType::JSON) {
        _discovery.printf_P(PSTR("\"%s\":"), name);
        // bool isBool = strcmp_P_P(str, PSTR("true")) == 0 || strcmp_P_P(str, PSTR("false")) == 0;
        if (quotes) {
            _discovery.print('"');
        }
        // _discovery.printf_P(PSTR("\"%s\":\""), name);
        #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
                auto len = strlen_P(RFPSTR(name));
                if (len > 2 && pgm_read_word(RFPSTR(name) + len - 2) == (('_') | ('t' << 8))) { // check if the name ends with "_t"
                    if (strncmp_P(_baseTopic.c_str(), str, _baseTopic.length()) == 0) { // replace _baseTopic with ~
                        str += _baseTopic.length();
                        _discovery.print('~');
                    }
                }
        #endif
        if (quotes) {
            JsonTools::Utf8Buffer buffer;
            JsonTools::printToEscaped(_discovery, FPSTR(str), &buffer);
            _discovery.print(F("\","));
        }
        else {
            _discovery.print(FPSTR(str));
            _discovery.print(',');
        }
    }
    else {
        _discovery.printf_P(PSTR("%s: %s\n    "), name, str);
    }
}

void Entity::finalize()
{
    if (!_discovery.length()) {
        __LDBG_printf("MQTT auto discovery payload empty");
        return;
    }
    if (_format == FormatType::JSON) {
        _discovery.remove(_discovery.length() - 1);
        _discovery += '}';
    }
    else {
        _discovery.remove(_discovery.length() - 4);
    }
    __LDBG_printf("MQTT auto discovery payload '%s'", printable_string(_discovery.c_str(), _discovery.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
}

const String Entity::_getUniqueId(const String &name)
{
    PrintString tmp;
    WebTemplate::printUniqueId(tmp, name);
    return tmp;
}
