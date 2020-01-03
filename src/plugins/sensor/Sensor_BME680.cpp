/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_BME680

#include "Sensor_BME680.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_BME680::Sensor_BME680(const String &name, uint8_t address) : MQTTSensor(), _name(name), _address(address)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_BME680(): component=%p\n"), this);
#endif
    registerClient(this);
     _bme680.begin(address);
}

void Sensor_BME680::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    String topic = MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str());

    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->addValueTemplate(F("temperature"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 1, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("%"));
    discovery->addValueTemplate(F("humidity"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 2, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("hPa"));
    discovery->addValueTemplate(F("pressure"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 3, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(emptyString);
    discovery->addValueTemplate(F("gas"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
}

uint8_t Sensor_BME680::getAutoDiscoveryCount() const {
    return 4;
}

void Sensor_BME680::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Sensor_BME680::getValues()\n"));

    auto sensor = _readSensor();

    auto obj = &array.addObject(4);
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
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("gas")));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(sensor.gas, 2));
}

void Sensor_BME680::createWebUI(WebUI &webUI, WebUIRow **row) {
    _debug_printf_P(PSTR("Sensor_BME680::createWebUI()\n"));

    *row = &webUI.addRow();

    (*row)->addSensor(_getId(F("temperature")), _name + F(" Temperature"), F("\u00b0C"));
    (*row)->addSensor(_getId(F("humidity")), _name + F(" Humidity"), F("%"));
    (*row)->addSensor(_getId(F("pressure")), _name + F(" Pressure"), F("hPa"));
    (*row)->addSensor(_getId(F("gas")), _name + F(" Gas"), emptyString);
}

void Sensor_BME680::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("BME680 @ I2C address 0x%02x" HTML_S(br)), _address);
}

Sensor_BME680::SensorEnumType_t Sensor_BME680::getType() const {
    return BME680;
}

void Sensor_BME680::publishState(MQTTClient *client) {
    if (client) {
        auto sensor = _readSensor();
        PrintString str;
        JsonUnnamedObject json;
        json.add(F("temperature"), JsonNumber(sensor.temperature, 2));
        json.add(F("humidity"), JsonNumber(sensor.humidity, 2));
        json.add(F("pressure"), JsonNumber(sensor.pressure, 2));
        json.add(F("gas"), JsonNumber(sensor.gas, 2));
        json.printTo(str);

        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str()), _qos, 1, str);
    }
}

Sensor_BME680::SensorData_t Sensor_BME680::_readSensor() {
    SensorData_t sensor;

    sensor.temperature = _bme680.readTemperature();
    sensor.humidity = _bme680.readHumidity();
    sensor.pressure = _bme680.readPressure() / 100.0;
    sensor.gas = _bme680.readGas();

    _debug_printf_P(PSTR("Sensor_BME680::_readSensor(): address 0x%02x: %.2f °C, %.2f%%, %.2f hPa, gas %u\n"), _address, sensor.temperature, sensor.humidity, sensor.pressure, sensor.gas);

    return sensor;
}

String Sensor_BME680::_getId(const __FlashStringHelper *type) {
    PrintString id(F("bme680_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
