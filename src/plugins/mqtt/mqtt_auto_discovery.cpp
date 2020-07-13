/**
 * Author: sascha_lammers@gmx.de
 */

// Yaml code for Home Assistant and auto discovery

#include <Arduino_compat.h>
#include <PrintString.h>
#include <JsonTools.h>
#include <kfc_fw_config.h>
#include "mqtt_auto_discovery.h"
#include "mqtt_component.h"
#include "mqtt_client.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void MQTTAutoDiscovery::create(MQTTComponent *component, const String &componentName, MQTTAutoDiscovery::FormatType format)
{
    String suffix = config.getDeviceName();
    if (componentName.length()) {
        suffix += '/';
        suffix += componentName;
    }
    _create(component, suffix, format);
}

void MQTTAutoDiscovery::_create(MQTTComponent *component, const String &name, MQTTAutoDiscovery::FormatType format)
{
    String uniqueId;

    _format = format;
    _topic = Config_MQTT::getDiscoveryPrefix();
    _topic += '/';
    _topic += component->getComponentName();
    _topic += '/';
    _topic += name;
    _topic += F("/config");

    _discovery = PrintString();
    if (_format == FormatType::JSON) {
        _discovery += '{';
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
        _baseTopic = MQTTClient::formatTopic(emptyString);
        addParameter(F("~"), _baseTopic);
#endif
    } else {
        _discovery += component->getComponentName();
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
        String model;
#if defined(MQTT_AUTO_DISCOVERY_MODEL)
        model = F(MQTT_AUTO_DISCOVERY_MODEL);
#elif IOT_SWITCH
    #if IOT_SWITCH_CHANNEL_NUM>1
        model = F(_STRINGIFY(IOT_SWITCH_CHANNEL_NUM) " Channel Switch");
    #else
        model = F("Switch");
    #endif
#elif IOT_DIMMER_MODULE
    #if IOT_DIMMER_MODULE_CHANNELS > 1
        model = F(_STRINGIFY(IOT_DIMMER_MODULE_CHANNELS) " Channel MOSFET Dimmer");
    #else
        model = F("MOSFET Dimmer");
    #endif
#else
        model = F("Generic");
#endif
#if defined(ESP8266)
        model += F("/ESP8266");
#elif defined(ESP32)
        model += F("/ESP32");
#elif defined(__AVR__) || defined(__avr__)
        model += F("/AVR");
#else
        model += F("/Unknown");
#endif

        #define JSON_VALUE_START            "\":\""
        #define JSON_NEXT_KEY_START         "\",\""

        _discovery.print(F("\"device\":{\"" MQTT_DEVICE_REG_IDENTIFIERS "\":[\""));
        _discovery.print(uniqueId);
        _discovery.print(F("\"],\"" MQTT_DEVICE_REG_CONNECTIONS "\":[[\"mac" JSON_NEXT_KEY_START));
        _discovery.print(WiFi.macAddress());
        _discovery.print(F("\"]],\"" MQTT_DEVICE_REG_MODEL JSON_VALUE_START));
        _discovery.print(model);
        _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_NAME JSON_VALUE_START));
        _discovery.print(config.getDeviceName());
        _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_SW_VERSION JSON_VALUE_START "KFC FW "));
        _discovery.print(KFCFWConfiguration::getFirmwareVersion());
        _discovery.print(F(JSON_NEXT_KEY_START MQTT_DEVICE_REG_MANUFACTURER JSON_VALUE_START "KFCLabs\"},"));
    }

    _debug_printf_P(PSTR("MQTT auto discovery topic '%s', name %s, number %d\n"), _topic.c_str(), component->getComponentName(), component->getNumber());
}

void MQTTAutoDiscovery::addParameter(const __FlashStringHelper *name, const String &value)
{
    if (_format == FormatType::JSON) {
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
        String valueAbbr = value;
        auto len = strlen_P(RFPSTR(name));
        if (len > 2 && pgm_read_word(RFPSTR(name) + len - 2) == (('_') | ('t' << 8))) { // check if the name ends with "_t"
            valueAbbr.replace(_baseTopic, String('~'));
        }
        _discovery.printf_P(PSTR("\"%s\":\"%s\","), name, valueAbbr.c_str());
#else
        _discovery.printf_P(PSTR("\"%s\":\"%s\","), name, value.c_str());
#endif
    }
    else {
        _discovery.printf_P(PSTR("%s: %s\n    "), name, value.c_str());
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
    if (_format == FormatType::JSON) {
        _discovery.remove(_discovery.length() - 1);
        _discovery += '}';
    } else {
        _discovery.remove(_discovery.length() - 4);
    }
    _debug_printf_P(PSTR("MQTT auto discovery payload '%s'\n"), printable_string(_discovery.c_str(), _discovery.length(), DEBUG_MQTT_CLIENT_PAYLOAD_LEN).c_str());
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
        config._H_GET(Config().flags).mqttAutoDiscoveryEnabled;
#else
    return false;
#endif
}

const String MQTTAutoDiscovery::_getUnqiueId(const String &name)
{
    uint16_t crc[4];

#if defined(ESP8266)

    typedef struct __attribute__packed__ {
        uint32_t chip_id;
        uint32_t flash_chip_id;
        uint8_t mac[2 * 6];
    } unique_device_info_t;
    unique_device_info_t info = { system_get_chip_id(), ESP.getFlashChipId()/* cached version of spi_flash_get_id() */ };
    wifi_get_macaddr(STATION_IF, info.mac);
    wifi_get_macaddr(SOFTAP_IF, info.mac + 6);

#elif defined(ESP32)

    typedef struct __attribute__packed__ {
        esp_chip_info_t chip_id;
        uint32_t flash_chip_id;
        uint8_t mac[4 * 6];
    } unique_device_info_t;
    unique_device_info_t info;

    esp_chip_info(&info.chip_id);
    info.flash_chip_id = ESP.getFlashChipSize();
    esp_read_mac(info.mac, ESP_MAC_WIFI_STA);
    esp_read_mac(info.mac + 6, ESP_MAC_WIFI_SOFTAP);
    esp_read_mac(info.mac + 12, ESP_MAC_BT);
    esp_read_mac(info.mac + 18, ESP_MAC_ETH);

#else
#error Platform not supported
#endif

    crc[0] = crc16_update(~0, &info, sizeof(info));
    crc[1] = crc16_update(crc[0], &info.chip_id, sizeof(info.chip_id));
    crc[1] = crc16_update(crc[1], &info.flash_chip_id, sizeof(info.flash_chip_id));
    crc[2] = crc16_update(crc[1], info.mac, sizeof(info.mac));
    crc[3] = crc16_update(~0, name.c_str(), name.length());

    PrintString uniqueId;
    for(uint8_t i = 0; i < 4; i++) {
        uniqueId.printf_P(PSTR("%04x"), crc[i]);
    }

    return uniqueId;
}
