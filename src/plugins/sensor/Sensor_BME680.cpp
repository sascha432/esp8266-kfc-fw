/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BME680

#include "Sensor_BME680.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_BME680::Sensor_BME680(const String &name, uint8_t address) : MQTT::Sensor(MQTT::SensorType::BME680), _name(name), _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_BME680::~Sensor_BME680()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_BME680::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
    case 0:
        if (discovery->create(this, _getId(FSPGM(temperature)), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            // discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
            discovery->addValueTemplate(FSPGM(temperature));
            discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
            discovery->addName(F("Temperature"));
            discovery->addObjectId(baseTopic + F("bme680_temp"));
        }
        break;
    case 1:
        if (discovery->create(this, _getId(FSPGM(humidity)), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            // discovery->addUnitOfMeasurement('%');
            discovery->addValueTemplate(FSPGM(humidity));
            discovery->addDeviceClass(F("humidity"), '%');
            discovery->addName(F("Humidity"));
            discovery->addObjectId(baseTopic + F("bme680_humidity"));
        }
        break;
    case 2:
        if (discovery->create(this, _getId(FSPGM(pressure)), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            // discovery->addUnitOfMeasurement(FSPGM(hPa));
            discovery->addValueTemplate(FSPGM(pressure));
            discovery->addDeviceClass(F("pressure"), FSPGM(hPa));
            discovery->addName(F("Pressure"));
            discovery->addObjectId(baseTopic + F("bme680_pressure"));
        }
        break;
    case 3:
        if (discovery->create(this, _getId(F("gas")), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            // discovery->addUnitOfMeasurement(F("ppm"));
            discovery->addValueTemplate(F("gas"));
            discovery->addDeviceClass(F("volatile_organic_compounds"), F("ppm"));
            discovery->addName(F("VOC Gas"));
            discovery->addObjectId(baseTopic + F("bme680_gas"));
        }
        break;
    }
    return discovery;
}

uint8_t Sensor_BME680::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_BME680::getValues(JsonArray &array, bool timer)
{
    __LDBG_printf("Sensor_BME680::getValues()");

    auto sensor = _readSensor();

    auto obj = &array.addObject(4);
    obj->add(JJ(id), _getId(FSPGM(temperature)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.temperature, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(humidity)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.humidity, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(pressure)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.pressure, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("gas")));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.gas, 2));
}

void Sensor_BME680::createWebUI(WebUINS::Root &webUI)
{
    __LDBG_printf("Sensor_BME680::createWebUI()");

    *row = &webUI.addRow();
    (*row)->addSensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(UTF8_degreeC));
    (*row)->addSensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%');
    (*row)->addSensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa));
    (*row)->addSensor(_getId(F("gas")), _name + F(" VOC Gas"), emptyString);
}

void Sensor_BME680::getStatus(Print &output)
{
    output.printf_P(PSTR("BME680 @ I2C address 0x%02x" HTML_S(br)), _address);
}

void Sensor_BME680::publishState(MQTT::Client *client)
{
    if (client && client->isConnected()) {
        auto sensor = _readSensor();
        PrintString str;
        JsonUnnamedObject json;
        json.add(FSPGM(temperature), JsonNumber(sensor.temperature, 2));
        json.add(FSPGM(humidity), JsonNumber(sensor.humidity, 2));
        json.add(FSPGM(pressure), JsonNumber(sensor.pressure, 2));
        json.add(F("gas"), JsonNumber(sensor.gas, 2));
        json.printTo(str);

        client->publish(MQTT::Client::formatTopic(_getId()), _qos, true, str);
    }
}

Sensor_BME680::SensorData_t Sensor_BME680::_readSensor()
{
    SensorData_t sensor;
    _bme680.begin(address);

    sensor.temperature = _bme680.readTemperature();
    sensor.humidity = _bme680.readHumidity();
    sensor.pressure = _bme680.readPressure() / 100.0;
    sensor.gas = _bme680.readGas();

    __LDBG_printf("Sensor_BME680::_readSensor(): address 0x%02x: %.2f °C, %.2f%%, %.2f hPa, gas %u", _address, sensor.temperature, sensor.humidity, sensor.pressure, sensor.gas);

    return sensor;
}

String Sensor_BME680::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("bme680_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
