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

Sensor_BME680::Sensor_BME680(const String &name, uint8_t address) : MQTTSensor(), _name(name), _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
    config.initTwoWire();
}

void Sensor_BME680::createAutoDiscovery(MQTTAutoDiscovery::FormatType format, MQTTAutoDiscoveryVector &vector)
{
    String topic = MQTTClient::formatTopic(_getId());

    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, _getId(FSPGM(temperature)), format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(FSPGM(_degreeC));
    discovery->addValueTemplate(FSPGM(temperature));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, _getId(FSPGM(humidity)), format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement('%');
    discovery->addValueTemplate(FSPGM(humidity));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, _getId(FSPGM(pressure)), format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(FSPGM(hPa));
    discovery->addValueTemplate(FSPGM(pressure));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, _getId(F("gas")), format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(emptyString);
    discovery->addValueTemplate(F("gas"));
    discovery->finalize();
    vector.emplace_back(discovery);
}

uint8_t Sensor_BME680::getAutoDiscoveryCount() const {
    return 4;
}

void Sensor_BME680::getValues(JsonArray &array, bool timer) {
    _debug_printf_P(PSTR("Sensor_BME680::getValues()\n"));

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

void Sensor_BME680::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_BME680::createWebUI()\n"));

    *row = &webUI.addRow();
    (*row)->addSensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(_degreeC));
    (*row)->addSensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%');
    (*row)->addSensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa));
    (*row)->addSensor(_getId(F("gas")), _name + F(" Gas"), emptyString);
}

void Sensor_BME680::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("BME680 @ I2C address 0x%02x" HTML_S(br)), _address);
}

MQTTSensorSensorType Sensor_BME680::getType() const
{
    return MQTTSensorSensorType::BME680;
}

void Sensor_BME680::publishState(MQTTClient *client)
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

        client->publish(MQTTClient::formatTopic(_getId()), _qos, true, str);
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

    _debug_printf_P(PSTR("Sensor_BME680::_readSensor(): address 0x%02x: %.2f °C, %.2f%%, %.2f hPa, gas %u\n"), _address, sensor.temperature, sensor.humidity, sensor.pressure, sensor.gas);

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
