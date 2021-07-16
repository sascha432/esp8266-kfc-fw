/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_CCS811

#include "Sensor_CCS811.h"
#include "Sensor_LM75A.h"
#include "Sensor_BME280.h"
#include "Sensor_BME680.h"
#include "sensor.h"

#undef DEBUG_IOT_SENSOR
#define DEBUG_IOT_SENSOR 1

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_CCS811::Sensor_CCS811(const String &name, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::CCS811),
    _name(name),
    _address(address)
{
    REGISTER_SENSOR_CLIENT(this);
    config.initTwoWire();
    _ccs811.begin(_address);
    setUpdateRate(max<uint16_t>(1, DEFAULT_UPDATE_RATE / 3)); // faster update rate until valid data is available
}

Sensor_CCS811::~Sensor_CCS811()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_CCS811::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(F("eco2")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(F("ppm"));
                discovery->addValueTemplate(F("eCO2"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(F("tvoc")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(F("ppb"));
                discovery->addValueTemplate(F("TVOC"));
            }
            break;
    }
    return discovery;
}

void Sensor_CCS811::getValues(WebUINS::Events &array, bool timer)
{
    auto &sensor = _readSensor();
    array.append(WebUINS::Values(_getId(F("eco2")), sensor.eCO2, sensor.available));
    array.append(WebUINS::Values(_getId(F("tvoc")), sensor.TVOC, sensor.available));
}

void Sensor_CCS811::createWebUI(WebUINS::Root &webUI)
{
    static constexpr auto renderType = IOT_SENSOR_CCS811_RENDER_TYPE;
    {
        auto sensor = WebUINS::Sensor(_getId(F("eco2")),  _name + F(" CO2"), F("ppm"), renderType);
        #ifdef IOT_SENSOR_CCS811_RENDER_HEIGHT
            sensor.append(WebUINS::NamedString(J(height), IOT_SENSOR_CCS811_RENDER_HEIGHT));
        #endif
        webUI.appendToLastRow(WebUINS::Row(sensor));
    }
    {
        auto sensor = WebUINS::Sensor(_getId(F("tvoc")),  _name + F(" TVOC"), F("ppb"), renderType);
        #ifdef IOT_SENSOR_CCS811_RENDER_HEIGHT
            sensor.append(WebUINS::NamedString(J(height), IOT_SENSOR_CCS811_RENDER_HEIGHT));
        #endif
        webUI.appendToLastRow(WebUINS::Row(sensor));
    }
}

void Sensor_CCS811::getStatus(Print &output)
{
    output.printf_P(PSTR("CCS811 @ I2C address 0x%02x" HTML_S(br)), _address);
}

void Sensor_CCS811::publishState()
{
    if (isConnected()) {
        auto &sensor = _readSensor();
        if (sensor.available) {
            using namespace MQTT::Json;
            publish(_getTopic(), true, UnnamedObject(NamedUnsignedShort(F("eCO2"), sensor.eCO2), NamedUnsignedShort(F("TVOC"), sensor.TVOC)).toString());
        }
    }
}

Sensor_CCS811::SensorData &Sensor_CCS811::_readSensor()
{
// use temperature and humidity for compensation
#if IOT_SENSOR_HAVE_LM75A || IOT_SENSOR_HAVE_BME280 || IOT_SENSOR_HAVE_BME680
    auto &sensors = SensorPlugin::getSensors();
    for(auto sensor: sensors) {
        #if IOT_SENSOR_HAVE_BME280
            if (sensor->getType() == SensorType::BME280) {
                auto data = reinterpret_cast<Sensor_BME280 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: BME280: humidity %u, temperature %.3f", (uint8_t)data.humidity, data.temperature);
                _ccs811.setEnvironmentalData(data.humidity, data.temperature);
            }
        #elif IOT_SENSOR_HAVE_BME680
            if (sensor->getType() == SensorType::BME680) {
                auto data = reinterpret_cast<Sensor_BME680 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: BME680: humidity %u, temperature %.3f", (uint8_t)data.humidity, data.temperature);
                _ccs811.setEnvironmentalData(data.humidity, data.temperature);
            }
        #elif IOT_SENSOR_HAVE_LM75A
            if (sensor->getType() == SensorType::LM75A) {
                auto temperature = reinterpret_cast<Sensor_BME680 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: LM75A: humidity fixed 55, temperature %.3f", temperature);
                _ccs811.setEnvironmentalData(55, temperature);
            }
        #endif
    }
#else
    _ccs811.setEnvironmentalData(55, 25);
#endif

    bool available = _ccs811.available() && (_ccs811.readData() == 0);
    if (!available) {
        __LDBG_printf("CCS811: address 0x%02x: available=0", _address, available);
        return _sensor;
    }

    _sensor.available = available;

    if (_sensor.available) {
        _sensor.eCO2 = _ccs811.geteCO2();
        _sensor.TVOC = _ccs811.getTVOC();
        if (!_sensor.eCO2 || !_sensor.TVOC) {
            _sensor.available = false;
        }
        else {
            setUpdateRate(DEFAULT_UPDATE_RATE);
        }
    }
    else {
        _sensor.eCO2 = 0;
        _sensor.TVOC = 0;
    }

    __LDBG_printf("CCS811: address 0x%02x: available=%u, eCO2 %uppm, TVOC %uppb, error=%u",
        _address, _sensor.available, _sensor.eCO2, _sensor.TVOC, _ccs811.checkError()
    );

    return _sensor;
}

String Sensor_CCS811::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("ccs811_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

#endif
