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

using namespace MQTT;

int16_t Sensor::_orderIdCounter = 10000;

Sensor::Sensor(SensorType type) :
    MQTTComponent(ComponentType::SENSOR),
    _updateRate(DEFAULT_UPDATE_RATE),
    _mqttUpdateRate(DEFAULT_MQTT_UPDATE_RATE),
    _nextUpdate(0),
    _nextMqttUpdate(0),
    _orderId(_getNextOrderId()),
    _type(type)
{
    __LDBG_printf("ctor this=%p", this);
}

Sensor::~Sensor()
{
    __LDBG_printf("dtor this=%p", this);
    shutdown();
    #if DEBUG
        if (hasClient() && client().isComponentRegistered(this)) {
            __DBG_panic("component=%p type=%d is still registered", this, getType());
        }
    #endif
}

void Sensor::onConnect()
{
    setNextMqttUpdate(2);
}

void Sensor::timerEvent(WebUINS::Events *array, bool mqttIsConnected)
{
    uint32_t now = time(nullptr);
    if (array && now >= _nextUpdate) {
        _nextUpdate = now + _updateRate;
        getValues(*array, true);
        // __DBG_printf("%s", array->toString().c_str());
    }
    if (mqttIsConnected && isConnected() && now >=_nextMqttUpdate) {
        _nextMqttUpdate = now + _mqttUpdateRate;
        publishState();
    }
}
