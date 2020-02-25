/**
 * Author: sascha_lammers@gmx.de
 */

// Yaml code for Home Assistant and auto discovery

#if MQTT_SUPPORT

#include <Arduino_compat.h>
#include <PrintString.h>
#include <JsonTools.h>
#include <kfc_fw_config.h>
#include "mqtt_auto_discovery.h"
#include "mqtt_component.h"
#include "mqtt_client.h"
#include "progmem_data.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void MQTTAutoDiscovery::create(MQTTComponent *component, uint8_t count, MQTTAutoDiscovery::Format_t format)
{
    String name = MQTTClient::getComponentName(component->getNumber() + count);
    String uniqueId;

    _format = format;
    _topic = PrintString(F("%s/%s/%s/config"), Config_MQTT::getDiscoveryPrefix(), component->getComponentName(), name.c_str());
    // _topic = Config_MQTT::getDiscoveryPrefix();
    // _topic += '/';
    // _topic += FPSTR(component->getComponentName());
    // _topic += '/';
    // _topic += name;
    // _topic += F("/config");

    _discovery = PrintString();
    if (_format == FORMAT_JSON) {
        _discovery += '{';
    } else {
        _discovery += FPSTR(component->getComponentName());
        _discovery += F(":\n  - ");
    }
    addParameter(F("name"), name);
    addParameter(F("platform"), F("mqtt"));
    if (format == FORMAT_JSON) {
        uniqueId = _getUnqiueId(name);
        addParameter(FSPGM(mqtt_unique_id), uniqueId);
    }
    addParameter(FSPGM(mqtt_availability_topic), MQTTClient::formatTopic(-1, FSPGM(mqtt_status_topic)));
    addParameter(FSPGM(mqtt_payload_available), FSPGM(1));
    addParameter(FSPGM(mqtt_payload_not_available), FSPGM(0));

    if (_format == FORMAT_JSON) {
        String model;
#if IOT_DIMMER_MODULE
    #if IOT_DIMMER_MODULE_CHANNELS
        model += F(_STRINGIFY(IOT_DIMMER_MODULE_CHANNELS) " Channel MOSFET Dimmer/");
    #else
        model += F("MOSFET Dimmer/");
    #endif
#elif IOT_ATOMIC_SUN_V2
        model += F("Atomic Sun V2/");
#else
        model = F("Generic/");
#endif
#if defined(ESP8266)
        model += F("ESP8266");
#elif defined(ESP32)
        model += F("ESP32");
#elif defined(__AVR__) || defined(__avr__)
        model += F("AVR");
#else
        model += F("Unknown");
#endif

        _discovery += F("\"device\":{\"identifiers\":[\"");
        _discovery.printf_P(PSTR("%s\"],\"name\":\"%s\",\"model\":\"%s\",\"sw_version\":\"KFC FW %s\",\"manufacturer\":\"KFCLabs\""), uniqueId.c_str(), name.c_str(), model.c_str(), KFCFWConfiguration::getFirmwareVersion().c_str());
        _discovery += F("},");
    }

    _debug_printf_P(PSTR("MQTT auto discovery topic '%s', name %s, number %d\n"), _topic.c_str(), component->getComponentName(), component->getNumber());
}

void MQTTAutoDiscovery::addParameter(const String &name, const String &value)
{
    if (_format == FORMAT_JSON) {
        _discovery.printf_P(PSTR("\"%s\":\"%s\","), name.c_str(), value.c_str());
    } else {
        _discovery.printf_P(PSTR("%s: %s\n    "), name.c_str(), value.c_str());
    }
}

void MQTTAutoDiscovery::addStateTopic(const String &value)
{
    addParameter(FSPGM(mqtt_state_topic), value);
}

void MQTTAutoDiscovery::addCommandTopic(const String &value)
{
    addParameter(FSPGM(mqtt_command_topic), value);
}

void MQTTAutoDiscovery::addPayloadOn(const String &value)
{
    addParameter(FSPGM(mqtt_payload_on), value);
}

void MQTTAutoDiscovery::addPayloadOff(const String &value)
{
    addParameter(FSPGM(mqtt_payload_off), value);
}

void MQTTAutoDiscovery::addBrightnessStateTopic(const String &value)
{
    addParameter(FSPGM(mqtt_brightness_state_topic), value);
}

void MQTTAutoDiscovery::addBrightnessCommandTopic(const String &value)
{
    addParameter(FSPGM(mqtt_brightness_command_topic), value);
}

void MQTTAutoDiscovery::addBrightnessScale(uint32_t brightness)
{
    addParameter(FSPGM(mqtt_brightness_scale), String(brightness));
}

void MQTTAutoDiscovery::addColorTempStateTopic(const String &value)
{
    addParameter(FSPGM(mqtt_color_temp_state_topic), value);
}

void MQTTAutoDiscovery::addColorTempCommandTopic(const String &value)
{
    addParameter(FSPGM(mqtt_color_temp_command_topic), value);
}

void MQTTAutoDiscovery::addRGBStateTopic(const String &value)
{
    addParameter(FSPGM(mqtt_rgb_state_topic), value);
}

void MQTTAutoDiscovery::addRGBCommandTopic(const String &value)
{
    addParameter(FSPGM(mqtt_rgb_command_topic), value);
}

void MQTTAutoDiscovery::addUnitOfMeasurement(const String &value)
{
    addParameter(FSPGM(mqtt_unit_of_measurement), value);
}

void MQTTAutoDiscovery::addValueTemplate(const String &value)
{
    PrintString value_json(F("{{ value_json.%s }}"), value.c_str());
    addParameter(FSPGM(mqtt_value_template), value_json);
}

void MQTTAutoDiscovery::finalize()
{
    if (_format == FORMAT_JSON) {
        _discovery.remove(_discovery.length() - 1);
        _discovery += '}';
    } else {
        _discovery.remove(_discovery.length() - 4);
    }
    _debug_printf_P(PSTR("MQTT auto discovery payload '%s'\n"), _discovery.c_str());
}

PrintString &MQTTAutoDiscovery::getPayload()
{
    return _discovery;
}

String &MQTTAutoDiscovery::getTopic()
{
    return _topic;
}

bool MQTTAutoDiscovery::isEnabled()
{
#if MQTT_AUTO_DISCOVERY
    return config._H_GET(Config().flags).mqttAutoDiscoveryEnabled;
#else
    return false;
#endif
}

const String MQTTAutoDiscovery::_getUnqiueId(const String &name)
{
    uint16_t crc[3];

#if defined(ESP8266)
    String deviceId = String(ESP.getChipId(), HEX);
    crc[0] = crc16_update(~0, (uint8_t *)deviceId.c_str(), deviceId.length());
    deviceId += String(ESP.getFlashChipId(), HEX);
    crc[1] = crc16_update(crc[0], (uint8_t *)deviceId.c_str(), deviceId.length());
#elif defined(ESP32)
    String deviceId = String(ESP.getChipRevision(), HEX);
    crc[0] = crc16_update(~0, (uint8_t *)deviceId.c_str(), deviceId.length());
    deviceId += String(ESP.getFlashChipSize(), HEX);
    crc[1] = crc16_update(crc[0], (uint8_t *)deviceId.c_str(), deviceId.length());
#else
#error Platform not supported
#endif

    deviceId += WiFi.macAddress();
    crc[2] = crc16_update(crc[1], (uint8_t *)deviceId.c_str(), deviceId.length());

    PrintString uniqueId;
    uniqueId += name;
    uniqueId += '_';
    for(uint8_t i = 0; i < 3; i++) {
        uniqueId.printf("%04x", crc[i]);
    }
    return uniqueId;
}

#endif
