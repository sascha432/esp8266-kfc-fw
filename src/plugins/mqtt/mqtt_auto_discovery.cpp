/**
 * Author: sascha_lammers@gmx.de
 */

// Yaml code for Home Assistant and auto discovery

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
#include "mqtt_auto_discovery.h"
#include "mqtt_component.h"
#include "mqtt_client.h"
#include "progmem_data.h"

void MQTTAutoDiscovery::create(MQTTComponent *component, Format_t format) {
    auto &config = _Config.get();
    String name = MQTTClient::getComponentName(component->getNumber());

    _format = format;
    _topic = config.mqtt_discovery_prefix;
    _topic += '/';
    _topic += FPSTR(component->getComponentName());
    _topic += '/';
    _topic += name;

    _discovery = PrintString();
    if (_format == FORMAT_JSON) {
        _discovery += '{';
    } else {
        _discovery += FPSTR(component->getComponentName());
        _discovery += ':';
        _discovery += F("\n  - ");
    }
    addParameter(F("name"), name);
    addParameter(F("platform"), F("mqtt"));
    String uniqueId = _getUnqiueId();
    addParameter(FSPGM(mqtt_unique_id), uniqueId);
    addParameter(FSPGM(mqtt_availability_topic), MQTTClient::formatTopic(-1, FSPGM(mqtt_status_topic)));
    addParameter(FSPGM(mqtt_payload_available), FSPGM(1));
    addParameter(FSPGM(mqtt_payload_not_available), FSPGM(0));

    if (_format == FORMAT_JSON) {
        _discovery += F("\"device\":{\"identifiers\":[\"");
        _discovery.printf_P(PSTR("%s\"],name=\"%s\",\"sw_version\":\"%s\""), uniqueId.c_str(), config.device_name, config_firmware_version().c_str());
        _discovery += F("},");
    }
}

void MQTTAutoDiscovery::addParameter(const String &name, const String &value) {
    if (_format == FORMAT_JSON) {
        _discovery.printf_P(PSTR("\"%s\":\"%s\","), name.c_str(), value.c_str());
    } else {
        _discovery.printf_P(PSTR("%s: %s\n    "), name.c_str(), value.c_str());
    }
}

void MQTTAutoDiscovery::finalize() {
    if (_format == FORMAT_JSON) {
        _discovery.remove(_discovery.length() - 1);
        _discovery += '}';
    } else {
        _discovery.remove(_discovery.length() - 4);
    }
}

String MQTTAutoDiscovery::getPayload() {
    return _discovery;
}

String MQTTAutoDiscovery::getTopic() {
    return _topic;
}

bool MQTTAutoDiscovery::isEnabled() {
#if MQTT_AUTO_DISCOVERY
    return _Config.getOptions().isMQTTAutoDiscovery();
#else
    return false;
#endif
}

const String MQTTAutoDiscovery::_getUnqiueId() {
    uint16_t crc[3];

    String deviceId = String(ESP.getChipId(), HEX);
    crc[0] = crc16_update(~0, (uint8_t *)deviceId.c_str(), deviceId.length());
    deviceId += String(ESP.getFlashChipId(), HEX);
    crc[1] = crc16_update(crc[0], (uint8_t *)deviceId.c_str(), deviceId.length());
    deviceId += WiFi.macAddress();
    crc[2] = crc16_update(crc[1], (uint8_t *)deviceId.c_str(), deviceId.length());

    PrintString uniqueId;
    uniqueId = _Config.get().device_name;
    uniqueId += '_';
    for(uint8_t i = 0; i < 3; i++) {
        uniqueId.printf("%04x", crc[i]);
    }
    return uniqueId;
}
