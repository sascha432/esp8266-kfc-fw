/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_DHTxx

#include <Arduino_compat.h>
#include "Sensor_DHTxx.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_DHTxx::Sensor_DHTxx(const String &name, uint8_t pin/*, uint8_t type*/) : MQTT::Sensor(MQTT::SensorType::DHTxx), _name(name), _pin(pin), _dht(pin)
//_type(type), _dht(_pin, _type)
{
    REGISTER_SENSOR_CLIENT(this);
    // _dht.begin();
}

Sensor_DHTxx::~Sensor_DHTxx()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(FSPGM(temperature)), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
                discovery->addValueTemplate(FSPGM(temperature));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(FSPGM(humidity)), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(F("%"));
                discovery->addValueTemplate(FSPGM(humidity));
            }
            break;
    }
    return discovery;
}

uint8_t Sensor_DHTxx::getAutoDiscoveryCount() const
{
    return 2;
}

void Sensor_DHTxx::getValues(NamedJsonArray &array, bool timer)
{

}

void Sensor_DHTxx::getValues(JsonArray &array, bool timer)
{
    SensorData_t sensor;
    _readSensor(sensor);

    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(temperature)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.temperature, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(humidity)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.humidity, 2));
}

void Sensor_DHTxx::createWebUI(WebUINS::Root &webUI)
{
    (*row)->addSensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(UTF8_degreeC));
    (*row)->addSensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%');
}

void Sensor_DHTxx::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_DHTxx " @ pin %d" HTML_S(br)), _pin);
}

bool Sensor_DHTxx::getSensorData(String &name, StringVector &values)
{
    name = F(IOT_SENSOR_NAMES_DHTxx);
    SensorData_t sensor;
    _readSensor(sensor);
    values.emplace_back(PrintString(F("%.2f &deg;C"), sensor.temperature));
    values.emplace_back(PrintString(F("%.2f %%"), sensor.humidity));
    return true;
}

void Sensor_DHTxx::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        SensorData_t sensor;
        _readSensor(sensor);
        PrintString str;
        JsonUnnamedObject json;
        json.add(FSPGM(temperature), JsonNumber(sensor.temperature, 2));
        json.add(FSPGM(humidity), JsonNumber(sensor.humidity, 2));
        json.printTo(str);

        client->publish(MQTTClient::formatTopic(_getId()), _qos, true, str);
    }
}

void Sensor_DHTxx::_readSensor(SensorData_t &sensor)
{
    byte temperature = 0;
    byte humidity = 0;
    if (_dht.read(&temperature, &humidity, NULL) != SimpleDHTErrSuccess) {
        sensor.temperature = NAN;
        sensor.humidity = NAN;
    }
    else {
         sensor.humidity = humidity;
         sensor.temperature = temperature;
    }

    // if (_dht.read()) {
    //     sensor.humidity = _dht.readHumidity();
    //     sensor.temperature = _dht.readTemperature();
    // }
    // else {
    //     sensor.temperature = NAN;
    //     sensor.humidity = NAN;
    // }
    __LDBG_printf("pin %d: %.2f °C, %.2f%%, %.2f hPa", _pin, sensor.temperature, sensor.humidity);
}

String Sensor_DHTxx::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("dht%u_%02x"), IOT_SENSOR_HAVE_DHTxx, _pin);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
