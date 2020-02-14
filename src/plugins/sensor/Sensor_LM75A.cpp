/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_LM75A

#include "Sensor_LM75A.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_LM75A::Sensor_LM75A(const JsonString &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _wire(wire), _address(address)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_LM75A(): component=%p\n"), this);
#endif
    registerClient(this);
    config.initTwoWire();
}

void Sensor_LM75A::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str()));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(discovery);
}

uint8_t Sensor_LM75A::getAutoDiscoveryCount() const
{
    return 1;
}

void Sensor_LM75A::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("Sensor_LM75A::getValues()\n"));
    auto &obj = array.addObject(3);
    auto temp = _readSensor();
    obj.add(JJ(id), _getId());
    obj.add(JJ(state), !isnan(temp));
    obj.add(JJ(value), JsonNumber(temp, 2));
}

void Sensor_LM75A::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_LM75A::createWebUI()\n"));
    // if ((*row)->size() > 3) {
    //     *row = &webUI.addRow();
    // }
    (*row)->addSensor(_getId(), _name, F("\u00b0C"));
}

void Sensor_LM75A::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str()), _qos, 1, String(_readSensor(), 2));
    }
}

void Sensor_LM75A::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("LM75A @ I2C address 0x%02x" HTML_S(br)), _address);
}

MQTTSensorSensorType Sensor_LM75A::getType() const
{
    return MQTTSensorSensorType::LM75A;
}

float Sensor_LM75A::_readSensor()
{
    _wire.beginTransmission(_address);
    _wire.write(0x00);
    if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, (uint8_t)2) == 2) {
        float temp = (((uint8_t)Wire.read() << 8) | (uint8_t)Wire.read()) / 256.0;
        _debug_printf_P(PSTR("Sensor_LM75A::_readSensor(): address 0x%02x: %.2f\n"), _address, temp);
        return temp;
    }
    _debug_printf_P(PSTR("Sensor_LM75A::_readSensor(): address 0x%02x: error\n"), _address);
    return NAN;
}

String Sensor_LM75A::_getId()
{
    return PrintString(F("lm75a_0x%02x"), _address);
}

#endif
