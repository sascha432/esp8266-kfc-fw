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

Sensor_INA219::Sensor_INA219(const JsonString &name, TwoWire &wire, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::INA219),
    _name(name),
    _address(address),
    _updateTimer(0),
    _holdPeakTimer(0),
    _Ipeak(NAN),
    _ina219(address)
{
    REGISTER_SENSOR_CLIENT(this);
    _ina219.begin(&config.initTwoWire());
    _ina219.setCalibration(IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES, IOT_SENSOR_INA219_R_SHUNT * 4);

    __LDBG_printf("Sensor_INA219::Sensor_INA219(): address=%x, voltage range=%x, gain=%x, shunt ADC resolution=%x", _address, IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES);

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
    auto discovery = __LDBG_new(AutoDiscovery::Entity);
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(VOLTAGE), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(VOLTAGE)));
                discovery->addUnitOfMeasurement('V');
                discovery->addDeviceClass(F("voltage"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(CURRENT), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(CURRENT)));
                discovery->addUnitOfMeasurement(F("mA"));
                discovery->addDeviceClass(F("current"));
            }
            break;
        case 2:
            if (discovery->create(this, _getId(POWER), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(POWER)));
                discovery->addUnitOfMeasurement(F("mW"));
                discovery->addDeviceClass(F("power"));
            }
            break;
        case 3:
            if (discovery->create(this, _getId(PEAK_CURRENT), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId(PEAK_CURRENT)));
                discovery->addUnitOfMeasurement(F("mA"));
                discovery->addDeviceClass(F("current"));
            }
            break;
    }
    return discovery;
}

uint8_t Sensor_INA219::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_INA219::getValues(NamedArray &array, bool timer)
{
    auto U = _data.U();
    auto I = _data.I();
    auto P = _data.P();

    using namespace WebUINS;

    array.append(
        Values(_getId(VOLTAGE), TrimmedDouble(U, 2)),
        Values(_getId(CURRENT), RoundedDouble(I)),
        Values(_getId(POWER), RoundedDouble(P)),
        Values(_getId(PEAK_CURRENT), RoundedDouble(_Ipeak))
    );

    if (timer) {
        _data = SensorData();
    }
}

void Sensor_INA219::createWebUI(WebUINS::Root &webUI)
{
    using namespace WebUINS;
    Row row(
        Sensor(_getId(VOLTAGE), _name, 'V'),
        Sensor(_getId(CURRENT), F("Current"), F("mA")),
        Sensor(_getId(POWER), F("Power"), F("mW")),
        Sensor(_getId(PEAK_CURRENT), F("Peak Current"), F("mA"))
    );
    webUI.appendToLastRow(row);
}

void Sensor_INA219::publishState()
{
    if (isConnected()) {
        publish(MQTTClient::formatTopic(_getId(VOLTAGE)), true, String(_mqttData.U(), 2));
        publish(MQTTClient::formatTopic(_getId(CURRENT)), true, String(_mqttData.I(), 0));
        publish(MQTTClient::formatTopic(_getId(POWER)), true, String(_mqttData.P(), 0));
        publish(MQTTClient::formatTopic(_getId(PEAK_CURRENT)), true, String(_Ipeak, 0));
        _mqttData = SensorData();
    }
}

void Sensor_INA219::getStatus(Print &output)
{
    output.printf_P(PSTR("INA219 @ I2C address 0x%02x" HTML_S(br)), _address);
}

String Sensor_INA219::_getId(SensorTypeEnum_t type)
{
    return PrintString(F("ina219_0x%02x_%c"), _address, type);
}

void Sensor_INA219::_loop()
{
    if (millis() > _updateTimer) {
        _updateTimer = millis() + IOT_SENSOR_INA219_READ_INTERVAL;
        float U = _ina219.getBusVoltage_V();
        float I = _ina219.getCurrent_mA();
        // reset peak current after IOT_SENSOR_INA219_PEAK_HOLD_TIME seconds or store new peak
        if (I > _Ipeak || millis() > _holdPeakTimer) {
            _Ipeak = I;
            _holdPeakTimer = millis() + IOT_SENSOR_INA219_PEAK_HOLD_TIME * 1000;
        }
        _data.add(U, I);
        _mqttData.add(U, I);
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
            return std::count_if(SensorPlugin::begin(), SensorPlugin::end(), [this, &serial](SensorPtr sensor) {
                if ( sensor->getType() == SensorType::INA219) {
                    auto &ina219 = *reinterpret_cast<Sensor_INA219 *>(sensor);
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
                }
            });
        };

        if (!timerPrintFunc(*timer)) {
            serial.printf_P(PSTR("+%s: No sensor found\n"), PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219));
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
