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

Sensor_LM75A::Sensor_LM75A(const JsonString &name, TwoWire &wire, uint8_t address) : MQTT::Sensor(SensorType::LM75A), _name(name), _wire(wire), _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
    config.initTwoWire();
}

Sensor_LM75A::~Sensor_LM75A()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_LM75A::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    auto discovery = __LDBG_new(MQTT::AutoDiscovery::Entity);
    switch(num) {
        case 0:
            discovery->create(this, _getId(), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
            discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
            break;
    }
    return discovery;
}

uint8_t Sensor_LM75A::getAutoDiscoveryCount() const
{
    return 1;
}
void Sensor_LM75A::getValues(NamedJsonArray &array, bool timer)
{
    using namespace MQTT::Json;
    auto temp = _readSensor();
    array.append(UnnamedObject(
            NamedString(F("id"), _getId()),
            NamedBool(F("state"), !isnan(temp)),
            NamedVariant<FStr, TrimmedDouble>(F("value"), TrimmedDouble(temp, 2))
        )
    );
}

void Sensor_LM75A::getValues(JsonArray &array, bool timer)
{
    auto &obj = array.addObject(3);
    auto temp = _readSensor();
    obj.add(JJ(id), _getId());
    obj.add(JJ(state), !isnan(temp));
    obj.add(JJ(value), JsonNumber(temp, 2));
}

void Sensor_LM75A::createWebUI(WebUINS::Root &webUI)
{
    // if ((*row)->size() > 3) {
    //     *row = &webUI.addRow();
    // }
    (*row)->addSensor(_getId(), _name, FSPGM(UTF8_degreeC));
}

void Sensor_LM75A::publishState()
{
    if (isConnected()) {
        publish(MQTTClient::formatTopic(_getId()), true, String(_readSensor(), 2));
    }
}

void Sensor_LM75A::getStatus(Print &output)
{
    output.printf_P(PSTR("LM75A @ I2C address 0x%02x" HTML_S(br)), _address);
#if 0
    for(uint8_t i = 0x48; i <= 0x4f; i++) {
        auto val = _readSensor(i);
        output.printf_P(PSTR("address 0x%02x = %f" HTML_S(br)), i, val);
    }
#endif
}

bool Sensor_LM75A::getSensorData(String &name, StringVector &values)
{
    name = std::move(_name.toString());
    values.emplace_back(PrintString(F("%.2f &deg;C"), _readSensor()));
    return true;
}

float Sensor_LM75A::_readSensor(uint8_t address)
{
    if (address == 255) {
        address = _address;
    }
    _wire.beginTransmission(address);
    _wire.write(0x00);
    if (_wire.endTransmission() == 0 && _wire.requestFrom(address, (uint8_t)2) == 2) {
        float temp = (((uint8_t)Wire.read() << 8) | (uint8_t)Wire.read()) / 256.0;
        __LDBG_printf("Sensor_LM75A::_readSensor(): address 0x%02x: %.2f", address, temp);
        return temp;
    }
    __LDBG_printf("Sensor_LM75A::_readSensor(): address 0x%02x: error", address);
    return NAN;
}

String Sensor_LM75A::_getId()
{
    return PrintString(F("lm75a_0x%02x"), _address);
}

#endif
