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

#include <debug_helper_enable_mem.h>

Sensor_BME280::Sensor_BME280(const String &name, TwoWire &wire, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::BME280),
    _name(name),
    _address(address),
    _callback(nullptr)
{
    REGISTER_SENSOR_CLIENT(this);
    _bme280.begin(_address, &config.initTwoWire());
}

Sensor_BME280::~Sensor_BME280()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_BME280::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = __LDBG_new(MQTT::AutoDiscovery::Entity);
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(FSPGM(temperature, "temperature")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(FSPGM(UTF8_degreeC));
                discovery->addValueTemplate(FSPGM(temperature));
                discovery->addDeviceClass(F("temperature"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(FSPGM(humidity, "humidity")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement('%');
                discovery->addValueTemplate(FSPGM(humidity));
                discovery->addDeviceClass(F("humidity"));
            }
            break;
        case 2:
            if (discovery->create(this, _getId(FSPGM(pressure, "pressure")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(FSPGM(hPa, "hPa"));
                discovery->addValueTemplate(FSPGM(pressure));
                discovery->addDeviceClass(F("pressure"));
            }
            break;
    }
    return discovery;
}

uint8_t Sensor_BME280::getAutoDiscoveryCount() const
{
    return 3;
}

void Sensor_BME280::getValues(NamedArray &array, bool timer)
{
    using namespace MQTT::Json;

    SensorData_t sensor;
    _readSensor(sensor);
    array.append(
        WebUINS::Values(_getId(FSPGM(temperature)), WebUINS::TrimmedDouble(sensor.temperature, 2), true),
        WebUINS::Values(_getId(FSPGM(humidity)), WebUINS::TrimmedDouble(sensor.humidity, 2), true),
        WebUINS::Values(_getId(FSPGM(pressure)), WebUINS::TrimmedDouble(sensor.pressure, 2), true)
    );
}

void Sensor_BME280::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(
        WebUINS::Sensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(UTF8_degreeC)),
        WebUINS::Sensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%'),
        WebUINS::Sensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa))
    );
    webUI.appendToLastRow(row);


    // if ((*row)->size() > 1) {
        // *row = &webUI.addRow();
    // // }
    // (*row)->addSensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(UTF8_degreeC));
    // (*row)->addSensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%');
    // (*row)->addSensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa));
}

void Sensor_BME280::getStatus(Print &output)
{
    output.printf_P(PSTR("BME280 @ I2C address 0x%02x" HTML_S(br)), _address);
}

bool Sensor_BME280::getSensorData(String &name, StringVector &values)
{
    name = F("BME280");
    SensorData_t sensor;
    _readSensor(sensor);
    values.emplace_back(PrintString(F("%.2f %s"), sensor.temperature, SPGM(UTF8_degreeC)));
    values.emplace_back(PrintString(F("%.2f %%"), sensor.humidity));
    values.emplace_back(PrintString(F("%.2f hPa"), sensor.pressure));
    return true;
}

void Sensor_BME280::publishState()
{
    if (isConnected()) {
        SensorData_t sensor;
        _readSensor(sensor);

        using namespace MQTT::Json;

        publish(MQTTClient::formatTopic(_getId()), true, UnnamedObject(
            NamedFormattedDouble(FSPGM(temperature), sensor.temperature, F("%.2f")),
            NamedFormattedDouble(FSPGM(humidity), sensor.humidity, F("%.2f")),
            NamedFormattedDouble(FSPGM(pressure), sensor.pressure, F("%.2f"))
        ).toString());
    }
}

void Sensor_BME280::_readSensor(SensorData_t &sensor)
{
    sensor.temperature = _bme280.readTemperature();
    sensor.humidity = _bme280.readHumidity();
    sensor.pressure = _bme280.readPressure() / 100.0;

    __LDBG_printf("address 0x%02x: %.2f %s, %.2f%%, %.2f hPa", _address, sensor.temperature, SPGM(UTF8_degreeC), sensor.humidity, sensor.pressure);

    if (_callback != nullptr) {
        _callback(sensor);
        __LDBG_printf("compensated %.2f %s, %.2f%%", sensor.temperature, SPGM(UTF8_degreeC), sensor.humidity);
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
