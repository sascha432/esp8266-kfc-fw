/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BME680

#include "Sensor_BME680.h"

#    if DEBUG_IOT_SENSOR
#        include <debug_helper_enable.h>
#    else
#        include <debug_helper_disable.h>
#    endif

Sensor_BME680::Sensor_BME680(const String &name, uint8_t address, TwoWire &wire) :
    MQTT::Sensor(MQTT::SensorType::BME680),
    _name(name),
    _address(address)
#if HAVE_ADAFRUIT_BME680_LIB
    ,
    _bme680(&wire)
    #endif
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
    #if HAVE_ADAFRUIT_BME680_LIB
        output.printf_P(PSTR("BME680 @ I2C address 0x%02x" HTML_S(br)), _address);
    #endif
    #if HAVE_BOSCHSENSORTEC_LIB
        output.printf_P(PSTR("BME680 @ I2C address 0x%02x, BSec " HTML_S(br)), _address);
        output.printf_P(PSTR("BSEC library version %u.%u.%u.%u" HTML_S(br)), iaqSensor.version.major, iaqSensor.version.minor, iaqSensor.version.major_bugfix, iaqSensor.version.minor_bugfix);
    #endif
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
    values.emplace_back(PrintString(F("%.6f kOhm"), sensor.gas));
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
            NamedFormattedDouble(F("gas"), sensor.gas, F("%.2f"))
        ).toString());
    }
}

#if HAVE_ADAFRUIT_BME680_LIB


    Sensor_BME680::SensorDataType &Sensor_BME680::_readSensor()
    {
        auto endTime = _bme680.beginReading();
        if (!endTime) {
            return _sensor;
        }
        if (!_bme680.endReading()) {
            return _sensor;
        }

        auto lastSuccesfulAccess = _sensor.getTimeSinceLastSuccess();

        _sensor = SensorDataType(
            _bme680.temperature + _cfg.temp_offset,
            _bme680.humidity + _cfg.humidity_offset,
            (_bme680.pressure / 100.0) + _cfg.pressure_offset,
            _bme680.gas_resistance / 1000.0
        );
        _sensor.setLastSuccess();

        // __DBG_printf("address 0x%02x: %.2f %s, %.2f%%, %.2f hPa %.6f kOhm  delay=%u", _address, _sensor.temperature, SPGM(UTF8_degreeC), _sensor.humidity, _sensor.pressure, _sensor.gas, lastSuccesfulAccess);

        return _sensor;
    }

#endif

#if HAVE_BOSCHSENSORTEC_LIB

    Sensor_BME680::SensorDataType &Sensor_BME680::_readSensor()
    {
        String output;
        if (iaqSensor.run()) { // If new data is available
            output += ", " + String(iaqSensor.rawTemperature);
            output += ", " + String(iaqSensor.pressure);
            output += ", " + String(iaqSensor.rawHumidity);
            output += ", " + String(iaqSensor.gasResistance);
            output += ", " + String(iaqSensor.iaq);
            output += ", " + String(iaqSensor.iaqAccuracy);
            output += ", " + String(iaqSensor.temperature);
            output += ", " + String(iaqSensor.humidity);
            output += ", " + String(iaqSensor.staticIaq);
            output += ", " + String(iaqSensor.co2Equivalent);
            output += ", " + String(iaqSensor.breathVocEquivalent);
            Serial.println(output);
        } else {
            if (iaqSensor.status != BSEC_OK) {
                if (iaqSensor.status < BSEC_OK) {
                    output = "BSEC error code : " + String(iaqSensor.status);
                    Serial.println(output);
                } else {
                    output = "BSEC warning code : " + String(iaqSensor.status);
                    Serial.println(output);
                }
            }

            if (iaqSensor.bme680Status != BME680_OK) {
                if (iaqSensor.bme680Status < BME680_OK) {
                    output = "BME680 error code : " + String(iaqSensor.bme680Status);
                    Serial.println(output);
                } else {
                    output = "BME680 warning code : " + String(iaqSensor.bme680Status);
                    Serial.println(output);
                }
            }
        }
        return _sensor;
    }

#endif


#endif
