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

Sensor_BME680::Sensor_BME680(const String &name, uint8_t address, TwoWire &wire) :
    MQTT::Sensor(MQTT::SensorType::BME680),
    _name(name),
    _address(address),
    _bme680(&wire)
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
            discovery->addValueTemplate(FSPGM(temperature));
            discovery->addDeviceClass(F("temperature"), FSPGM(UTF8_degreeC));
            discovery->addName(F("Temperature"));
            discovery->addObjectId(baseTopic + F("bme680_temp"));
        }
        break;
    case 1:
        if (discovery->create(this, _getId(FSPGM(humidity)), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            discovery->addValueTemplate(FSPGM(humidity));
            discovery->addDeviceClass(F("humidity"), '%');
            discovery->addName(F("Humidity"));
            discovery->addObjectId(baseTopic + F("bme680_humidity"));
        }
        break;
    case 2:
        if (discovery->create(this, _getId(FSPGM(pressure)), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            discovery->addValueTemplate(FSPGM(pressure));
            discovery->addDeviceClass(F("pressure"), FSPGM(hPa));
            discovery->addName(F("Pressure"));
            discovery->addObjectId(baseTopic + F("bme680_pressure"));
        }
        break;
    case 3:
        if (discovery->create(this, _getId(F("gas")), format)) {
            discovery->addStateTopic(MQTT::Client::formatTopic(_getId()));
            discovery->addValueTemplate(F("gas"));
            discovery->addDeviceClass(F("volatile_organic_compounds"), F("ppm"));
            discovery->addName(F("VOC Gas"));
            discovery->addObjectId(baseTopic + F("bme680_gas"));
        }
        break;
    }
    return discovery;
}

void Sensor_BME680::getValues(WebUINS::Events &array, bool timer)
{
    using namespace MQTT::Json;

    auto sensor = _readSensor();
    array.append(
        WebUINS::Values(_getId(FSPGM(temperature)), WebUINS::TrimmedFloat(sensor.temperature, 2), true),
        WebUINS::Values(_getId(FSPGM(humidity)), WebUINS::TrimmedFloat(sensor.humidity, 2), true),
        WebUINS::Values(_getId(FSPGM(pressure)), WebUINS::TrimmedFloat(sensor.pressure, 2), true),
        NamedUint32(_getId(F("gas")), sensor.pressure)
    );
}

void Sensor_BME680::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(FSPGM(temperature)), _name + F(" Temperature"), FSPGM(UTF8_degreeC)).setConfig(_renderConfig)));
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(FSPGM(humidity)), _name + F(" Humidity"), '%').setConfig(_renderConfig)));
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(FSPGM(pressure)), _name + F(" Pressure"), FSPGM(hPa)).setConfig(_renderConfig)));
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(F("gas")), _name + F(" VOC Gas"), F("ppm")).setConfig(_renderConfig)));
}

void Sensor_BME680::getStatus(Print &output)
{
    output.printf_P(PSTR("BME680 @ I2C address 0x%02x" HTML_S(br)), _address);
    if (_cfg.temp_offset) {
        output.printf_P(PSTR("Temperature offset %.*f%s "), countDecimalPlaces(_cfg.temp_offset), _cfg.temp_offset, SPGM(UTF8_degreeC));
    }
    if (_cfg.humidity_offset) {
        output.printf_P(PSTR("Humidity offset %.*f%% "), countDecimalPlaces(_cfg.humidity_offset), _cfg.humidity_offset);
    }
    if (_cfg.pressure_offset) {
        output.printf_P(PSTR("Pressure offset %.*f%s"), countDecimalPlaces(_cfg.pressure_offset), _cfg.pressure_offset, FSPGM(hPa));
    }
    if (_cfg.temp_offset || _cfg.humidity_offset || _cfg.pressure_offset) {
        output.print(F(HTML_S(br)));
    }
}

bool Sensor_BME680::getSensorData(String &name, StringVector &values)
{
    name = F("BME680");
    auto sensor = _readSensor();
    values.emplace_back(PrintString(F("%.2f %s"), sensor.temperature, SPGM(UTF8_degreeC)));
    values.emplace_back(PrintString(F("%.2f %%"), sensor.humidity));
    values.emplace_back(PrintString(F("%.2f hPa"), sensor.pressure));
    values.emplace_back(PrintString(F("%u ppm"), sensor.gas));
    return true;
}

void Sensor_BME680::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();

    auto &group = form.addCardGroup(F("bme680"), F("BME680 Temperature, Humidity and Pressure Sensor"), true);

    form.addObjectGetterSetter(F("bme680_t"), FormGetterSetter(cfg.bme680, temp_offset));
    form.addFormUI(F("Temperature Offset"), FormUI::Suffix(FSPGM(UTF8_degreeC)));

    form.addObjectGetterSetter(F("bme680_h"), FormGetterSetter(cfg.bme680, humidity_offset));
    form.addFormUI(F("Humidity Offset"), FormUI::Suffix(F("%")));

    form.addObjectGetterSetter(F("bme680_p"), FormGetterSetter(cfg.bme680, pressure_offset));
    form.addFormUI(F("Pressure Offset"), FormUI::Suffix(FSPGM(hPa)));

    group.end();
}

void Sensor_BME680::publishState()
{
    if (isConnected()) {
        auto sensor = _readSensor();
        using namespace MQTT::Json;

        publish(MQTT::Client::formatTopic(_getId()), true, UnnamedObject(
            NamedFormattedDouble(FSPGM(temperature), sensor.temperature, F("%.2f")),
            NamedFormattedDouble(FSPGM(humidity), sensor.humidity, F("%.2f")),
            NamedFormattedDouble(FSPGM(pressure), sensor.pressure, F("%.2f")),
            NamedUint32(F("gas"), sensor.gas)
        ).toString());
    }
}

Sensor_BME680::SensorDataType Sensor_BME680::_readSensor()
{
    auto sensor = SensorDataType(
        _bme680.readTemperature() + _cfg.temp_offset,
        _bme680.readHumidity() + _cfg.humidity_offset,
        (_bme680.readPressure() / 100.0) + _cfg.pressure_offset,
        _bme680.readGas()
    );

    __LDBG_printf("address 0x%02x: %.2f %s, %.2f%%, %.2f hPa %u ppm", _address, sensor.temperature, SPGM(UTF8_degreeC), sensor.humidity, sensor.pressure, sensor.gas);

    return sensor;
}

#endif
