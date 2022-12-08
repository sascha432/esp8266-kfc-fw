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

Sensor_LM75A::Sensor_LM75A(const String &name, uint8_t address, TwoWire &wire) :
    MQTT::Sensor(SensorType::LM75A),
    _name(name),
    _wire(&wire),
    _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_LM75A::~Sensor_LM75A()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_LM75A::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    switch(num) {
        case 0:
            if (!discovery->create(this, _getId(), format)) {
                return discovery;
            }
            discovery->addName(F("LM75A Temperature"));
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
            discovery->addObjectId(MQTT::Client::getBaseTopicPrefix() + F("lm75a_temperature"));
            break;
    }
    return discovery;
}

uint8_t Sensor_LM75A::getAutoDiscoveryCount() const
{
    return 1;
}

void Sensor_LM75A::getValues(WebUINS::Events &array, bool timer)
{
    auto temp = _readSensor();
    array.append(WebUINS::Values(_getId(), WebUINS::TrimmedFloat(temp, 2)));
}

void Sensor_LM75A::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(), _name, FSPGM(UTF8_degreeC)).setConfig(_renderConfig)));
}

void Sensor_LM75A::publishState()
{
    if (isConnected()) {
        publish(MQTT::Client::formatTopic(_getId()), true, String(_readSensor(), 2));
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
    name = _name;
    values.emplace_back(PrintString(F("%.2f &deg;C"), _readSensor()));
    return true;
}

float Sensor_LM75A::_readSensor()
{
    _wire->beginTransmission(_address);
    _wire->write(0);
    if (_wire->endTransmission(false) == 0 && _wire->requestFrom(_address, 2U) == 2) {
        return ((_wire->read() << 8) | _wire->read()) / 256.0f;
    }
    return NAN;
}

String Sensor_LM75A::_getId()
{
    return PrintString(F("lm75a_0x%02x"), _address);
}

#endif
