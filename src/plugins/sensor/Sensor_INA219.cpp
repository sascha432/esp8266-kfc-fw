/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_INA219

#include <Arduino_compat.h>
#include "Sensor_INA219.h"
#include "sensor.h"
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include "WebUIComponent.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

Sensor_INA219::Sensor_INA219(const String &name, TwoWire &wire, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::INA219),
    _name(name),
    _address(address),
    _updateTimer(0),
    _holdPeakTimer(0),
    _data(IN219_WEBUI_UPDATE_RATE * 1000 / 2),
    _mqttData(_mqttUpdateRate * 1000 / 2),
    _Ipeak(NAN),
    _ina219(address)
{
    REGISTER_SENSOR_CLIENT(this);
    _ina219.begin(&config.initTwoWire());
    _ina219.setCalibration(IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES, IOT_SENSOR_INA219_R_SHUNT * 4);

    __LDBG_printf("address=%x voltage_range=%x gain=%x shunt_ADC_res=%x", _address, IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES);

    setUpdateRate(IN219_WEBUI_UPDATE_RATE);
    LoopFunctions::add([this]() {
        this->_loop();
    }, this);
}

Sensor_INA219::~Sensor_INA219()
{
    LoopFunctions::remove(this);
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_INA219::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(SensorInputType::VOLTAGE), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(SensorInputType::VOLTAGE)));
                discovery->addUnitOfMeasurement('V');
                discovery->addDeviceClass(F("voltage"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(SensorInputType::CURRENT), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(SensorInputType::CURRENT)));
                discovery->addUnitOfMeasurement(F("mA"));
                discovery->addDeviceClass(F("current"));
            }
            break;
        case 2:
            if (discovery->create(this, _getId(SensorInputType::POWER), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(SensorInputType::POWER)));
                discovery->addUnitOfMeasurement(F("mW"));
                discovery->addDeviceClass(F("power"));
            }
            break;
        case 3:
            if (discovery->create(this, _getId(SensorInputType::PEAK_CURRENT), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(SensorInputType::PEAK_CURRENT)));
                discovery->addUnitOfMeasurement(F("mA"));
                discovery->addDeviceClass(F("current"));
            }
            break;
    }
    return discovery;
}

void Sensor_INA219::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        WebUINS::Values(_getId(SensorInputType::VOLTAGE), WebUINS::TrimmedDouble(_data.U(), 2)),
        WebUINS::Values(_getId(SensorInputType::CURRENT), WebUINS::RoundedDouble(_data.I())),
        WebUINS::Values(_getId(SensorInputType::POWER), WebUINS::RoundedDouble(_data.P())),
        WebUINS::Values(_getId(SensorInputType::PEAK_CURRENT), WebUINS::RoundedDouble(_Ipeak))
    );
    // __LDBG_printf("U=%f I=%f P=%f %s", _data.U(), _data.I(), _data.P(), array.toString().c_str());
}

void Sensor_INA219::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(
        WebUINS::Sensor(_getId(SensorInputType::VOLTAGE), _name, 'V'),
        WebUINS::Sensor(_getId(SensorInputType::CURRENT), F("Current"), F("mA")),
        WebUINS::Sensor(_getId(SensorInputType::POWER), F("Power"), F("mW")),
        WebUINS::Sensor(_getId(SensorInputType::PEAK_CURRENT), F("Peak Current"), F("mA"))
    ));
}

void Sensor_INA219::publishState()
{
    if (isConnected()) {
        publish(MQTTClient::formatTopic(_getId(SensorInputType::VOLTAGE)), true, String(_mqttData.U(), 2));
        publish(MQTTClient::formatTopic(_getId(SensorInputType::CURRENT)), true, String(_mqttData.I(), 0));
        publish(MQTTClient::formatTopic(_getId(SensorInputType::POWER)), true, String(_mqttData.P(), 0));
        publish(MQTTClient::formatTopic(_getId(SensorInputType::PEAK_CURRENT)), true, String(_Ipeak, 0));
    }
}

void Sensor_INA219::getStatus(Print &output)
{
    output.printf_P(PSTR("INA219 @ I2C address 0x%02x, shunt %.3fm\xE2\x84\xA6" HTML_S(br)), _address, IOT_SENSOR_INA219_R_SHUNT * 1000.0);
}

String Sensor_INA219::_getId(SensorInputType type)
{
    return PrintString(F("ina219_0x%02x_%c"), _address, type);
}

void Sensor_INA219::_loop()
{
    if (millis() > _updateTimer) {
        _updateTimer = millis() + IOT_SENSOR_INA219_READ_INTERVAL;
        auto microsTime = micros();
        float U = _ina219.getBusVoltage_V();
        float I = _ina219.getCurrent_mA();
        // reset peak current after IOT_SENSOR_INA219_PEAK_HOLD_TIME milliseconds or store new peak
        if (I > _Ipeak || millis() > _holdPeakTimer) {
            _Ipeak = I;
            _holdPeakTimer = millis() + IOT_SENSOR_INA219_PEAK_HOLD_TIME;
        }
        _data.add(U, I, microsTime);
        _mqttData.add(U, I, microsTime);
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORINA219, "SENSORINA219", "<interval in ms>", "Print INA219 sensor data");

void Sensor_INA219::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219), SensorPlugin::getInstance().getName_P());
}

bool Sensor_INA219::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219))) {

        static Event::Timer timer;
        timer.remove();

        auto &serial = args.getStream();
        auto timerPrintFunc = [this, &serial](Event::CallbackTimerPtr timer) {
            return std::count_if(SensorPlugin::begin(), SensorPlugin::end(), [this, &serial](SensorPtr sensorPtr) {
                if (sensorPtr->getType() == SensorType::INA219) {
                    auto &sensor = *reinterpret_cast<Sensor_INA219 *>(sensorPtr);
                    auto &ina219 = sensor._ina219;
                    serial.printf_P(PSTR("+SENSORINA219: raw: U=%d, Vshunt=%d, I=%d, current: P=%d: %.3fV, %.1fmA, %.1fmW, average: %.3fV, %.1fmA, %.1fmW\n"),
                        ina219.getBusVoltage_raw(),
                        ina219.getShuntVoltage_raw(),
                        ina219.getCurrent_raw(),
                        ina219.getPower_raw(),
                        ina219.getBusVoltage_V(),
                        ina219.getCurrent_mA(),
                        ina219.getBusVoltage_V() * ina219.getCurrent_mA(),
                        sensor.getVoltage(),
                        sensor.getCurrent(),
                        sensor.getPower()
                    );
                    return true;
                }
                return false;
            });
        };

        if (!timerPrintFunc(*timer)) {
            args.printf_P(PSTR("No sensor found"));
        }
        else {
            auto repeat = args.toMillis(AtModeArgs::FIRST, 500, ~0, 0, String('s'));
            if (repeat) {
                _Timer(timer).add(repeat, true, timerPrintFunc);
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
