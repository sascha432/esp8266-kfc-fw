/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BME680

#include "Sensor_BME680.h"

#define DEBUG_IOT_SENSOR_BME680 1

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
        , _bme680(&wire)
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
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
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
        WebUINS::Values(_getId(F("gas")), WebUINS::TrimmedFloat(sensor.gas, 0), true)
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
    if (isnan(_sensor.gas)) { // only read the sensor data if not valid, otherwise the mqtt publish interval is used to keep the sensor data consistent
        _readSensor();
    }
    values.emplace_back(PrintString(F("%.2f %s"), _sensor.temperature, SPGM(UTF8_degreeC)));
    values.emplace_back(PrintString(F("%.2f %%"), _sensor.humidity));
    values.emplace_back(PrintString(F("%.2f hPa"), _sensor.pressure));
    values.emplace_back(PrintString(F("%.6f ppm"), _sensor.gas));
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
    auto sensor = _readSensor(); // always read state to get it for the webui
    if (isConnected()) {
        using namespace MQTT::Json;

        publish(MQTT::Client::formatTopic(_getId()), true, UnnamedObject(
            NamedFormattedDouble(FSPGM(temperature), sensor.temperature, F("%.1f")),
            NamedFormattedDouble(FSPGM(humidity), sensor.humidity, F("%.1f")),
            NamedFormattedDouble(FSPGM(pressure), sensor.pressure, F("%.1f")),
            NamedFormattedDouble(F("gas"), sensor.gas, F("%.1f"))
        ).toString());
    }
}

#if HAVE_ADAFRUIT_BME680_LIB

    void Sensor_BME680::setup()
    {
        _readConfig();
        if (!_bme680.begin(_address)) {
            return;
        }
        _bme680.setTemperatureOversampling(BME680_OS_8X);
        _bme680.setHumidityOversampling(BME680_OS_2X);
        _bme680.setPressureOversampling(BME680_OS_4X);
        _bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
        _bme680.setGasHeater(320, 150); // 320*C for 150 ms
    }


    Sensor_BME680::SensorDataType &Sensor_BME680::_readSensor()
    {
        auto endTime = _bme680.beginReading();
        if (!endTime) {
            __DBG_printf("begin reading failed");
            return _sensor;

        }
        if (!_bme680.endReading()) {
            __DBG_printf("end reading failed");
            return _sensor;
        }

        // Air Quality Approximation
        float baselineResistance = 50000;  // Initial baseline gas resistance (Ohms)
        float baselineCO2 = 400;           // Start assuming fresh air at 400 ppm
        float alpha = 0.02;                // Smoothing factor for baseline adaptation

        float gasResistance = _bme680.gas_resistance;  // Read gas resistance (Ω)

        // Adapt baseline over time (slowly adjust to stable air conditions)
        baselineResistance = baselineResistance * (1 - alpha) + gasResistance * alpha;

        // Normalize gas resistance relative to baseline
        float resistanceRatio = baselineResistance / gasResistance;

        // Approximate VOC Index (0-500 scale)
        float vocIndex = constrain(100 * (resistanceRatio - 1), 0, 500);

        // Estimate CO2 (ppm) using an exponential relationship
        float estimatedCO2 = baselineCO2 * exp(vocIndex / 100.0); // Exponential scaling

        _sensor = SensorDataType(
            _bme680.temperature + _cfg.temp_offset,
            _bme680.humidity + _cfg.humidity_offset,
            (_bme680.pressure / 100.0) + _cfg.pressure_offset,
            estimatedCO2
        );

        return _sensor;
    }

#endif

#if HAVE_BOSCHSENSORTEC_LIB

    void Sensor_BME680::setup()
    {
        _readConfig();
        iaqSensor.begin(_address, config.initTwoWire());

        bsec_virtual_sensor_t sensorList[] = {
            BSEC_OUTPUT_RAW_TEMPERATURE,
            BSEC_OUTPUT_RAW_PRESSURE,
            BSEC_OUTPUT_RAW_HUMIDITY,
            BSEC_OUTPUT_RAW_GAS,
            // BSEC_OUTPUT_IAQ,
            // BSEC_OUTPUT_STATIC_IAQ,
            BSEC_OUTPUT_CO2_EQUIVALENT,
            BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
            BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
            BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        };

        iaqSensor.updateSubscription(sensorList, sizeof(sensorList) / sizeof(sensorList[0]), BSEC_SAMPLE_RATE_LP);

        _readSensor();
    }

    Sensor_BME680::SensorDataType &Sensor_BME680::_readSensor()
    {
        if (iaqSensor.run()) { // If new data is available
            #if DEBUG_IOT_SENSOR_BME680
                String output;
                output += "BSEC " + String(iaqSensor.rawTemperature);
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
            #endif

            _sensor = SensorDataType(
                iaqSensor.temperature + _cfg.temp_offset,
                iaqSensor.humidity + _cfg.humidity_offset,
                (iaqSensor.pressure / 100.0) + _cfg.pressure_offset,
                iaqSensor.co2Equivalent
            );
            _sensor.setLastSuccess();

        } else {
            #if DEBUG_IOT_SENSOR_BME680
                String output;
                if (iaqSensor.bsecStatus != BSEC_OK) {
                    if (iaqSensor.bsecStatus < BSEC_OK) {
                        output = "BSEC error code : " + String(iaqSensor.bsecStatus);
                        Serial.println(output);
                    } else {
                        output = "BSEC warning code : " + String(iaqSensor.bsecStatus);
                        Serial.println(output);
                    }
                }

                if (iaqSensor.bme68xStatus != BME68X_OK) {
                    if (iaqSensor.bme68xStatus < BME68X_OK) {
                        output = "BME680 error code : " + String(iaqSensor.bme68xStatus);
                        Serial.println(output);
                    } else {
                        output = "BME680 warning code : " + String(iaqSensor.bme68xStatus);
                        Serial.println(output);
                    }
                }
            #endif
            if (_sensor.getTimeSinceLastSuccess() > 1000 * 60) {
                _sensor = SensorDataType();
            }
        }
        return _sensor;
    }

#endif


#endif
