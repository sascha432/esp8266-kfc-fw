/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "MQTTSensor.h"
#include <time.h>

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

MQTTSensor::MQTTSensor() : MQTTComponent(ComponentTypeEnum_t::SENSOR), _updateRate(DEFAULT_UPDATE_RATE), _mqttUpdateRate(DEFAULT_MQTT_UPDATE_RATE), _nextUpdate(0), _nextMqttUpdate(0)
{
    _debug_println();
}

MQTTSensor::~MQTTSensor()
{
#if DEBUG
    auto client = MQTTClient::getClient();
    if (client && client->isComponentRegistered(this)) {
        __debugbreak_and_panic_printf_P(PSTR("component=%p type=%d is still registered\n"), this, (int)getType());
    }
#endif
}

void MQTTSensor::onConnect(MQTTClient *client)
{
    setNextMqttUpdate(2);
}


void MQTTSensor::timerEvent(JsonArray &array)
{
    auto currentTime = time(nullptr);
    if (currentTime >= _nextUpdate) {
        _nextUpdate = currentTime + _updateRate;

        _debug_println();
        getValues(array, true);
    }
    if (currentTime >=_nextMqttUpdate) {
        _nextMqttUpdate = currentTime + _mqttUpdateRate;

        _debug_println();
        publishState(MQTTClient::getClient());
    }
}
