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

MQTTSensor::MQTTSensor() :
    MQTTComponent(ComponentType::SENSOR),
    _updateRate(DEFAULT_UPDATE_RATE),
    _mqttUpdateRate(DEFAULT_MQTT_UPDATE_RATE),
    _nextUpdate(0),
    _nextMqttUpdate(0)
{
    __LDBG_printf("ctor this=%p", this);
}

MQTTSensor::~MQTTSensor()
{
    __LDBG_printf("dtor this=%p", this);
#if DEBUG
    auto client = MQTTClient::getClient();
    if (client && client->isComponentRegistered(this)) {
        __DBG_panic("component=%p type=%d is still registered", this, (int)getType());
    }
#endif
}

void MQTTSensor::onConnect(MQTTClient *client)
{
    setNextMqttUpdate(2);
}

void MQTTSensor::timerEvent(JsonArray *array, MQTTClient *client)
{
    if (!array && !client) {
        return;
    }

    uint32_t now = time(nullptr);
    if (array && now >= _nextUpdate) {
        _nextUpdate = now + _updateRate;
        _debug_println();
        getValues(*array, true);
    }

    if (client && client->isConnected() && now >=_nextMqttUpdate) {
        _nextMqttUpdate = now + _mqttUpdateRate;
        publishState(client);
    }
}
