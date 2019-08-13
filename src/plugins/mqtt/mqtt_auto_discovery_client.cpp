/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#include "mqtt_auto_discovery_client.h"
#include "mqtt_client.h"
#include "JsonCallbackReader.h"
#include <PrintString.h>
#include <StreamString.h>

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

uint16_t MQTTAutoDiscoveryClient::_uniqueId = 1;
MQTTAutoDiscoveryClient *MQTTAutoDiscoveryClient::_instance = nullptr;

PROGMEM_STRING_DEF(mqtt_auto_discovery_client, "MQTT auto discovery: ");

MQTTAutoDiscoveryClient::MQTTAutoDiscoveryClient(Stream *stream) : MQTTComponent(BINARY_SENSOR), _stream(stream) {
    _debug_println(F("MQTTAutoDiscoveryClient::MQTTAutoDiscoveryClient()"));
    auto client = MQTTClient::getClient();
    if (client && client->isConnected()) { // already connected? emulate onConnect callback
        onConnect(client);
    }
}

MQTTAutoDiscoveryClient *MQTTAutoDiscoveryClient::createInstance(Stream *stream) {
    deleteInstance();
    return (_instance = new MQTTAutoDiscoveryClient(stream));
}

void MQTTAutoDiscoveryClient::deleteInstance() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

MQTTAutoDiscoveryClient::~MQTTAutoDiscoveryClient() {
    _debug_println(F("MQTTAutoDiscoveryClient::~MQTTAutoDiscoveryClient()"));
    auto client = MQTTClient::getClient();
    if (client) {
        client->remove(this);
    }
}

void MQTTAutoDiscoveryClient::onConnect(MQTTClient *client) {
    _discovery.clear();

    String topic = config._H_STR(Config().mqtt_discovery_prefix);
    topic += '/';
    topic += '#';
    client->subscribe(this, topic, 0);
}

void MQTTAutoDiscoveryClient::onDisconnect(MQTTClient *client, AsyncMqttClientDisconnectReason reason) {
    _discovery.clear();
}

void MQTTAutoDiscoveryClient::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
    _debug_printf_P(PSTR("MQTTAutoDiscoveryClient::onMessage('%s', '%*.*s')\n"), topic, len, len, payload);

    StreamString stream;
    stream.write(reinterpret_cast<const uint8_t *>(payload), len);

    auto discoveryPtr = std::unique_ptr<Discovery_t>(new Discovery_t);
    auto &discovery = *discoveryPtr;
    discovery.id = _uniqueId;
    discovery.topic = topic;
    discovery.payloadLength = len;

    JsonCallbackReader reader(stream, [&discovery](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) {
        DEBUG_JSON_CALLBACK_P(_debug_printf_P, key, value, json);

        if (key.equals(F("name")) && json.getLevel() == 1) {
            discovery.name = value;
        }
        else if (json.getLevel() == 2 && json.getPath(1).equals(F("device"))) {
            if (key.equals(F("sw_version"))) {
                discovery.swVersion = value;
            }
            else if (key.equals(F("model"))) {
                discovery.model = value;
            }
            else if (key.equals(F("manufacturer"))) {
                discovery.manufacturer = value;
            }
            else if (key.equals(F("name")) && !discovery.name.length()) { // do not override name 
                discovery.name = value;
            }
        }
        return true;
    });
    reader.parse();

    // check if we have that topic already
    for(auto iterator = _discovery.begin(); iterator != _discovery.end(); ++iterator) {
        if ((*iterator)->topic.equals(discovery.topic)) {
            if (discovery.payloadLength) {
                // we keep the id, name and update the other info
                discovery.id = (*iterator)->id;
                discovery.name = (*iterator)->name;
                if (_stream) {
                    _stream->print(FSPGM(mqtt_auto_discovery_client));
                    _stream->print(F("Device updated: "));
                    _stream->println(discovery.name);
                    _stream->flush();
                }
                (*iterator).swap(discoveryPtr);
            }
            else {
                // no payload, remove device
                if (_stream) {
                    _stream->print(FSPGM(mqtt_auto_discovery_client));
                    _stream->print(F("Device removed: "));
                    _stream->println((*iterator)->name);
                    _stream->flush();
                }
                _discovery.erase(iterator);
            }
            return;
        }
    }

    if (!discovery.name.length()) {
        discovery.name = PrintString(F("<unnamed %u>"), discovery.id);
    }

    if (_stream) {
        _stream->print(FSPGM(mqtt_auto_discovery_client));
        _stream->print(F("New device: "));
        _stream->println(discovery.name);
        _stream->flush();
    }

    _uniqueId++; // increase counter for each new device
    _discovery.push_back(std::move(discoveryPtr));
}

#endif
