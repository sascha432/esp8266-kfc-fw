/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BME280

#include <Arduino_compat.h>
#include "Sensor_BME280.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_BME280::Sensor_BME280(const String &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _address(address), _callback(nullptr)
{
    REGISTER_SENSOR_CLIENT(this);
    _bme280.begin(_address, &config.initTwoWire());
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_BME280::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    String topic = MQTTClient::formatTopic(MQTTClient::NO_ENUM, FSPGM(__s_), _getId().c_str());
    switch(num) {
        case 0:
            discovery->create(this, _getId(FSPGM(temperature, "temperature")), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(F("\u00b0C"));
            discovery->addValueTemplate(FSPGM(temperature));
            break;
        case 1:
            discovery->create(this, _getId(FSPGM(humidity, "humidity")), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('%');
            discovery->addValueTemplate(FSPGM(humidity));
            break;
        case 2:
            discovery->create(this, _getId(FSPGM(pressure, "pressure")), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(FSPGM(hPa, "hPa"));
            discovery->addValueTemplate(FSPGM(pressure));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_BME280::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_BME280::getValues(JsonArray &array, bool timer)
{
    _debug_println();

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
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(pressure)));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.pressure, 2));
}

void Sensor_BME280::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_println();
    // if ((*row)->size() > 1) {
        // *row = &webUI.addRow();
    // }
    (*row)->addSensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), F("\u00b0C"));
    (*row)->addSensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%');
    (*row)->addSensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa));
}

void Sensor_BME280::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("BME280 @ I2C address 0x%02x" HTML_S(br)), _address);
}

MQTTSensorSensorType Sensor_BME280::getType() const
{
    return MQTTSensorSensorType::BME280;
}

bool Sensor_BME280::getSensorData(String &name, StringVector &values)
{
    name = F("BME280");
    SensorData_t sensor;
    _readSensor(sensor);
    values.emplace_back(PrintString(F("%.2f °C"), sensor.temperature));
    values.emplace_back(PrintString(F("%.2f %%"), sensor.humidity));
    values.emplace_back(PrintString(F("%.2f hPa"), sensor.pressure));
    return true;
}

void Sensor_BME280::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        SensorData_t sensor;
        _readSensor(sensor);
        PrintString str;
        JsonUnnamedObject json;
        json.add(FSPGM(temperature), JsonNumber(sensor.temperature, 2));
        json.add(FSPGM(humidity), JsonNumber(sensor.humidity, 2));
        json.add(FSPGM(pressure), JsonNumber(sensor.pressure, 2));
        json.printTo(str);

        auto _qos = MQTTClient::getDefaultQos();
        client->publish(MQTTClient::formatTopic(MQTTClient::NO_ENUM, FSPGM(__s_), _getId().c_str()), _qos, 1, str);
    }
}

void Sensor_BME280::_readSensor(SensorData_t &sensor)
{
    sensor.temperature = _bme280.readTemperature();
    sensor.humidity = _bme280.readHumidity();
    sensor.pressure = _bme280.readPressure() / 100.0;

    _debug_printf_P(PSTR("address 0x%02x: %.2f °C, %.2f%%, %.2f hPa\n"), _address, sensor.temperature, sensor.humidity, sensor.pressure);

    if (_callback != nullptr) {
        _callback(sensor);
        _debug_printf_P(PSTR("compensated %.2f °C, %.2f%%\n"), sensor.temperature, sensor.humidity);
    }
}

String Sensor_BME280::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("bme280_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
