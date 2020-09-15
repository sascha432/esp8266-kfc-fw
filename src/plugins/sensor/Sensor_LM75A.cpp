/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LM75A

#include "Sensor_LM75A.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

Sensor_LM75A::Sensor_LM75A(const JsonString &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _wire(wire), _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
    config.initTwoWire();
}

Sensor_LM75A::~Sensor_LM75A()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_LM75A::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, _getId(), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
            discovery->addUnitOfMeasurement(FSPGM(_degreeC));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_LM75A::getAutoDiscoveryCount() const
{
    return 1;
}

void Sensor_LM75A::getValues(JsonArray &array, bool timer)
{
    _debug_println();
    auto &obj = array.addObject(3);
    auto temp = _readSensor();
    obj.add(JJ(id), _getId());
    obj.add(JJ(state), !isnan(temp));
    obj.add(JJ(value), JsonNumber(temp, 2));
}

void Sensor_LM75A::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    _debug_println();
    // if ((*row)->size() > 3) {
    //     *row = &webUI.addRow();
    // }
    (*row)->addSensor(_getId(), _name, FSPGM(_degreeC));
}

void Sensor_LM75A::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(_getId()), true, String(_readSensor(), 2));
    }
}

void Sensor_LM75A::getStatus(Print &output)
{
    output.printf_P(PSTR("LM75A @ I2C address 0x%02x" HTML_S(br)), _address);
}

MQTTSensorSensorType Sensor_LM75A::getType() const
{
    return MQTTSensorSensorType::LM75A;
}

bool Sensor_LM75A::getSensorData(String &name, StringVector &values)
{
    name = F("LM75A");
    values.emplace_back(PrintString(F("%.2f °C"), _readSensor()));
    return true;
}

float Sensor_LM75A::_readSensor()
{
    _wire.beginTransmission(_address);
    _wire.write(0x00);
    if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, (uint8_t)2) == 2) {
        float temp = (((uint8_t)Wire.read() << 8) | (uint8_t)Wire.read()) / 256.0;
        __LDBG_printf("Sensor_LM75A::_readSensor(): address 0x%02x: %.2f", _address, temp);
        return temp;
    }
    __LDBG_printf("Sensor_LM75A::_readSensor(): address 0x%02x: error", _address);
    return NAN;
}

String Sensor_LM75A::_getId()
{
    return PrintString(F("lm75a_0x%02x"), _address);
}

#endif
