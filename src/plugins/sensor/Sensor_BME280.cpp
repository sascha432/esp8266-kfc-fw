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

Sensor_BME280::Sensor_BME280(const String &name, TwoWire &wire, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::BME280),
    _name(name),
    _address(address),
    _callback(nullptr)
{
    REGISTER_SENSOR_CLIENT(this);
    _readConfig();
    _bme280.begin(_address, &config.initTwoWire());
}

Sensor_BME280::~Sensor_BME280()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_BME280::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
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

void Sensor_BME280::getValues(WebUINS::Events &array, bool timer)
{
    auto sensor = _readSensor();
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
    if (_cfg.temp_offset) {
        output.printf_P(PSTR("Temperature offset %.3f%s "), _cfg.temp_offset, SPGM(UTF8_degreeC));
    }
    if (_cfg.humidity_offset) {
        output.printf_P(PSTR("Humidity offset %.3f%% "), _cfg.humidity_offset);
    }
    if (_cfg.pressure_offset) {
        output.printf_P(PSTR("Pressure offset %.3f%s"), _cfg.pressure_offset, FSPGM(hPa));
    }
}

bool Sensor_BME280::getSensorData(String &name, StringVector &values)
{
    name = F("BME280");
    auto sensor = _readSensor();
    values.emplace_back(PrintString(F("%.2f %s"), sensor.temperature, SPGM(UTF8_degreeC)));
    values.emplace_back(PrintString(F("%.2f %%"), sensor.humidity));
    values.emplace_back(PrintString(F("%.2f hPa"), sensor.pressure));
    return true;
}

void Sensor_BME280::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();

    auto &group = form.addCardGroup(F("bme280"), F("BME280 Temperature, Humidity and Pressure Sensor"), true);

    form.addPointerTriviallyCopyable<float>(F("bme280_t"), reinterpret_cast<void *>(&cfg.bme280.temp_offset));
    form.addFormUI(F("Temperature Offset"), FormUI::Suffix(FSPGM(UTF8_degreeC)));

    form.addPointerTriviallyCopyable<float>(F("bme280_h"), reinterpret_cast<void *>(&cfg.bme280.humidity_offset));
    form.addFormUI(F("Humidity Offset"), FormUI::Suffix(F("%")));

    form.addPointerTriviallyCopyable<float>(F("bme280_p"), reinterpret_cast<void *>(&cfg.bme280.pressure_offset));
    form.addFormUI(F("Pressure Offset"), FormUI::Suffix(FSPGM(hPa)));

    group.end();
}

void Sensor_BME280::publishState()
{
    if (isConnected()) {
        auto sensor = _readSensor();

        using namespace MQTT::Json;

        publish(MQTTClient::formatTopic(_getId()), true, UnnamedObject(
            NamedFormattedDouble(FSPGM(temperature), sensor.temperature, F("%.2f")),
            NamedFormattedDouble(FSPGM(humidity), sensor.humidity, F("%.2f")),
            NamedFormattedDouble(FSPGM(pressure), sensor.pressure, F("%.2f"))
        ).toString());
    }
}

Sensor_BME280::SensorDataType Sensor_BME280::_readSensor()
{
    SensorDataType sensor;

    sensor.temperature = _bme280.readTemperature() + _cfg.temp_offset;
    sensor.humidity = _bme280.readHumidity() + _cfg.humidity_offset;
    sensor.pressure = (_bme280.readPressure() / 100.0) + _cfg.pressure_offset;

    __LDBG_printf("address 0x%02x: %.2f %s, %.2f%%, %.2f hPa", _address, sensor.temperature, SPGM(UTF8_degreeC), sensor.humidity, sensor.pressure);

#if IOT_SENSOR_BME280_HAVE_COMPENSATION_CALLBACK
    if (_callback) {
        _callback(sensor);
        __LDBG_printf("compensated %.2f %s, %.2f%%", sensor.temperature, SPGM(UTF8_degreeC), sensor.humidity);
    }
#endif
    return sensor;
}


#endif
