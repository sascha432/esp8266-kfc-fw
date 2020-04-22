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

void Sensor_BME280::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    _debug_println();
    String topic = MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str());

    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->addValueTemplate(F("temperature"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, 1, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("%"));
    discovery->addValueTemplate(F("humidity"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, 2, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("hPa"));
    discovery->addValueTemplate(F("pressure"));
    discovery->finalize();
    vector.emplace_back(discovery);
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
    obj->add(JJ(id), _getId(F("temperature")));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.temperature, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("humidity")));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.humidity, 2));
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("pressure")));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.pressure, 2));
}

void Sensor_BME280::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_BME280::createWebUI()\n"));
    // if ((*row)->size() > 1) {
        // *row = &webUI.addRow();
    // }
    (*row)->addSensor(_getId(F("temperature")), _name + F(" Temperature"), F("\u00b0C"));
    (*row)->addSensor(_getId(F("humidity")), _name + F(" Humidity"), F("%"));
    (*row)->addSensor(_getId(F("pressure")), _name + F(" Pressure"), F("hPa"));
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
        json.add(F("temperature"), JsonNumber(sensor.temperature, 2));
        json.add(F("humidity"), JsonNumber(sensor.humidity, 2));
        json.add(F("pressure"), JsonNumber(sensor.pressure, 2));
        json.printTo(str);

        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str()), _qos, 1, str);
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
