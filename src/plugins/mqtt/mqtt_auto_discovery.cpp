/**
 * Author: sascha_lammers@gmx.de
 */

// Yaml code for Home Assistant and auto discovery

#include <Arduino_compat.h>
#include <PrintString.h>
#include <JsonTools.h>
#include <kfc_fw_config.h>
#include "mqtt_strings.h"
#include "mqtt_auto_discovery.h"
#include "mqtt_client.h"
#include "templates.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

bool MQTTAutoDiscovery::create(MQTTComponent *component, const String &componentName, FormatType format)
{
    return create(component->getType(), componentName, format);
}

bool MQTTAutoDiscovery::create(ComponentType componentType, const String &componentName, FormatType format)
{
    String suffix = System::Device::getName();
    if (componentName.length()) {
        if (!String_startsWith(componentName, '/')) {
            suffix += '/';
        }
        suffix += componentName;
    }
    return _create(componentType, suffix, format);
}

String MQTTAutoDiscovery::getWildcardTopic()
{
    return PrintString(F("%s/+/%s/#"), MQTTClient::ClientConfig::getAutoDiscoveryPrefix(), System::Device::getName());
}

bool MQTTAutoDiscovery::_create(ComponentType componentType, const String &name, FormatType format)
{
    String uniqueId;

    _format = format;
    _topic = MQTTClient::ClientConfig::getAutoDiscoveryPrefix();
    _topic += '/';
    _topic += MQTTComponent::getNameByType(componentType);
    if (!String_startsWith(name, '/')) {
        _topic += '/';
    }
    _topic += name;
    if (!String_endsWith(_topic, '/')) {
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
        _baseTopic = MQTTClient::formatTopic(emptyString);
        addParameter(F("~"), _baseTopic);
#endif
    } else {
        _discovery += MQTTComponent::getNameByType(componentType);
        _discovery += F(":\n  - ");
    }
    addParameter(FSPGM(name), name);
    addParameter(F("platform"), FSPGM(mqtt));
    if (format == FormatType::JSON) {
        uniqueId = _getUnqiueId(name);
        addParameter(FSPGM(mqtt_unique_id), uniqueId);
    }
    addParameter(FSPGM(mqtt_availability_topic), MQTTClient::formatTopic(FSPGM(mqtt_status_topic)));
    addParameter(FSPGM(mqtt_payload_available), 1);
    addParameter(FSPGM(mqtt_payload_not_available), 0);
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

void MQTTAutoDiscovery::__addParameter(const __FlashStringHelper *name, const char *str, bool list)
{
    if (_format == FormatType::TOPIC) {
        return;
    }
    else if (_format == FormatType::JSON) {
        _discovery.printf_P(PSTR("\"%s\":"), name);
        if (!list) {
            _discovery.print('"');
        }
        // _discovery.printf_P(PSTR("\"%s\":\""), name);
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
        auto len = strlen_P(RFPSTR(name));
        if (len > 2 && pgm_read_word(RFPSTR(name) + len - 2) == (('_') | ('t' << 8))) { // check if the name ends with "_t"
            if (strncmp_P(_baseTopic.c_str(), str, _baseTopic.length()) == 0) { // replace _baseTopic withj ~
                str += _baseTopic.length();
                _discovery.print('~');
            }
        }
#endif
        _discovery.print(FPSTR(str));
        if (!list) {
            _discovery.print('"');
        }
        _discovery.print(',');
    }
    else {
        _discovery.printf_P(PSTR("%s: %s\n    "), name, str);
    }
}

void MQTTAutoDiscovery::finalize()
{
    if (!_discovery.length()) {
        __LDBG_printf("MQTT auto discovery payload empty");
        return;
    }
    if (_format == FormatType::JSON) {
        _discovery.remove(_discovery.length() - 1);
        _discovery += '}';
    } else {
        _discovery.remove(_discovery.length() - 4);
    }
    __LDBG_printf("MQTT auto discovery payload '%s'", printable_string(_discovery.c_str(), _discovery.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
}

PrintString &MQTTAutoDiscovery::getPayload()
{
    return _discovery;
}

String &MQTTAutoDiscovery::getTopic()
{
    return _topic;
}

size_t MQTTAutoDiscovery::getMessageSize() const
{
    return _discovery.length() + _topic.length() + 16;
}

bool MQTTAutoDiscovery::isEnabled()
{
#if MQTT_AUTO_DISCOVERY
    return
#if ENABLE_DEEP_SLEEP
        !resetDetector.hasWakeUpDetected() &&
#endif
        System::Flags::getConfig().is_mqtt_enabled;
#else
    return false;
#endif
}

const String MQTTAutoDiscovery::_getUnqiueId(const String &name)
{
    PrintString tmp;
    WebTemplate::printUniqueId(tmp, name);
    return tmp;
}
