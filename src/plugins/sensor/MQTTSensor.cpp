/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR

#include "MQTTSensor.h"
#include <time.h>

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

MQTTSensor::MQTTSensor() : MQTTComponent(SENSOR), _updateRate(60) {
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
    _nextUpdate = 0;
}

MQTTSensor::~MQTTSensor() {
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
    }
}

void MQTTSensor::onConnect(MQTTClient *client) {
    _qos = MQTTClient::getDefaultQos();
#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        MQTTAutoDiscoveryVector vector;
        createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON, vector);
        for(auto &&discovery: vector) {
            _debug_printf_P(PSTR("MQTTSensor::onConnect(): topic=%s, payload=%s\n"), discovery->getTopic().c_str(), discovery->getPayload().c_str());
            client->publish(discovery->getTopic(), _qos, true, discovery->getPayload());
        }
    }
#endif
    publishState(client);
}

void MQTTSensor::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
}

void MQTTSensor::timerEvent(JsonArray &array) {
    unsigned long currentTime = time(nullptr);
    if (currentTime > _nextUpdate) {
        _nextUpdate = currentTime + _updateRate;

        publishState(MQTTClient::getClient());
        getValues(array);
    }
}

#endif
