/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR

#include "Sensor_BME280.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_BME280::Sensor_BME280(const String &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _address(address) {
     _bme280.begin(address, &wire);
     _topic = MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str());
}

void Sensor_BME280::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addStateTopic(_topic);
    discovery->addUnitOfMeasurement(F("°C"));
    discovery->addValueTemplate(F("temperature"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addStateTopic(_topic);
    discovery->addUnitOfMeasurement(F("%"));
    discovery->addValueTemplate(F("humidity"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addStateTopic(_topic);
    discovery->addUnitOfMeasurement(F("hPa"));
    discovery->addValueTemplate(F("pressure"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
}

void Sensor_BME280::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Sensor_BME280::getValues()\n"));

    auto sensor = _readSensor();

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

void Sensor_BME280::createWebUI(WebUI &webUI, WebUIRow **row) {
    _debug_printf_P(PSTR("Sensor_BME280::createWebUI()\n"));

    (*row)->addSensor(_getId(F("temperature")), _name + F(" Temperature"), JF("°C"));
    (*row)->addSensor(_getId(F("humidity")), _name + F(" Humidity"), JF("%"));
    (*row)->addSensor(_getId(F("pressure")), _name + F(" Pressure"), JF("hPa"));
}

void Sensor_BME280::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("BME280, I2C address 0x%02x" HTML_S(br)), _address);
}

void Sensor_BME280::publishState(MQTTClient *client) {
    if (client) {
        auto sensor = _readSensor();
        PrintString str;
        JsonUnnamedObject json;
        json.add(JF("temperature"), JsonNumber(sensor.temperature, 2));
        json.add(JF("humidity"), JsonNumber(sensor.humidity, 2));
        json.add(JF("pressure"), JsonNumber(sensor.pressure, 2));
        json.printTo(str);
        client->publish(_topic, _qos, 1, str);
    }
}

Sensor_BME280::SensorData_t Sensor_BME280::_readSensor() {
    SensorData_t sensor;

    sensor.temperature = _bme280.readTemperature();
    sensor.humidity = _bme280.readHumidity();
    sensor.pressure = _bme280.readPressure() / 100.0;

    debug_printf_P(PSTR("Sensor_BME280::_readSensor(): address 0x%02x: %.2f °C, %.2f%%, %.2f hPa\n"), _address, sensor.temperature, sensor.humidity, sensor.pressure);

    return sensor;
}

String Sensor_BME280::_getId(const __FlashStringHelper *type) {
    PrintString id(F("bme280_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
