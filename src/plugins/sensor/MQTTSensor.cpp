/**
 * Author: sascha_lammers@gmx.de
 */

#include "MQTTSensor.h"
#include <time.h>

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

MQTTSensor::MQTTSensor() : MQTTComponent(SENSOR), _updateRate(DEFAULT_UPDATE_RATE)
{
    _debug_println();
    _nextUpdate = 0;
}

MQTTSensor::~MQTTSensor()
{
    _debug_println();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
    }
}

void MQTTSensor::onConnect(MQTTClient *client)
{
    _debug_println();
    _qos = MQTTClient::getDefaultQos();
#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        MQTTAutoDiscoveryVector vector;
        createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON, vector);
        for(const auto &discovery: vector) {
            _debug_printf_P(PSTR("topic=%s, payload=%s\n"), discovery->getTopic().c_str(), discovery->getPayload().c_str());
            client->publish(discovery->getTopic(), _qos, true, discovery->getPayload());
        }
    }
#endif
    publishState(client);
}

void MQTTSensor::timerEvent(JsonArray &array)
{
    auto currentTime = time(nullptr);
    if (currentTime > _nextUpdate) {
        _nextUpdate = currentTime + _updateRate;

        _debug_println();
        publishState(MQTTClient::getClient());
        getValues(array);
    }
}
