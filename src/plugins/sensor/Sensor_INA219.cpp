/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_INA219

#include "Sensor_INA219.h"
#include "sensor.h"
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <MicrosTimer.h>

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_INA219::Sensor_INA219(const JsonString &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _address(address), _ina219(address)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_INA219(): component=%p\n"), this);
#endif
    registerClient(this);
    _ina219.begin(&config.initTwoWire());
    _ina219.setCalibration(IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES, IOT_SENSOR_INA219_R_SHUNT);

    _debug_printf_P(PSTR("Sensor_INA219::Sensor_INA219(): address=%x, voltage range=%x, gain=%x, shunt ADC resolution=%x\n"), _address, IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES);
    _Uint = NAN;
    _Iint = NAN;
    _Pint = NAN;
    _Ipeak = NAN;
    _holdPeakTimer = 0;
    _updateTimer = 0;
    setUpdateRate(IN219_UPDATE_RATE);
    LoopFunctions::add([this]() {
        this->_loop();
    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

Sensor_INA219::~Sensor_INA219()
{
    LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

void Sensor_INA219::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId(VOLTAGE).c_str()));
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 1, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId(CURRENT).c_str()));
    discovery->addUnitOfMeasurement(F("mA"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 2, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId(POWER).c_str()));
    discovery->addUnitOfMeasurement(F("mW"));
    discovery->finalize();
    vector.emplace_back(discovery);


    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 3, format);
    discovery->addStateTopic(MQTTClient::formatTopic(-1, F("/%s/"), _getId(PEAK_CURRENT).c_str()));
    discovery->addUnitOfMeasurement(F("mA"));
    discovery->finalize();
    vector.emplace_back(discovery);
}

uint8_t Sensor_INA219::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_INA219::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("Sensor_INA219::getValues()\n"));
    auto *obj = &array.addObject(3);
    obj->add(JJ(id), _getId(VOLTAGE));
    obj->add(JJ(state), !isnan(_Uint));
    obj->add(JJ(value), JsonNumber(_Uint, 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(CURRENT));
    obj->add(JJ(state), !isnan(_Iint));
    obj->add(JJ(value), JsonNumber(_Iint, 0));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(POWER));
    obj->add(JJ(state), !isnan(_Pint));
    obj->add(JJ(value), JsonNumber(_Pint, 0));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(PEAK_CURRENT));
    obj->add(JJ(state), !isnan(_Ipeak));
    obj->add(JJ(value), JsonNumber(_Ipeak, 0));
}

void Sensor_INA219::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_INA219::createWebUI()\n"));
    (*row)->addSensor(_getId(VOLTAGE), _name, F("V"));
    (*row)->addSensor(_getId(CURRENT), F("Current"), F("mA"));
    (*row)->addSensor(_getId(POWER), F("Power"), F("mW"));
    (*row)->addSensor(_getId(PEAK_CURRENT), F("Peak Current"), F("mA"));
}

void Sensor_INA219::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId(VOLTAGE).c_str()), _qos, 1, String(_Uint, 2));
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId(CURRENT).c_str()), _qos, 1, String(_Iint, 0));
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId(POWER).c_str()), _qos, 1, String(_Pint, 0));
        client->publish(MQTTClient::formatTopic(-1, F("/%s/"), _getId(PEAK_CURRENT).c_str()), _qos, 1, String(_Ipeak, 0));
    }
}

void Sensor_INA219::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("INA219 @ I2C address 0x%02x" HTML_S(br)), _address);
}

MQTTSensorSensorType Sensor_INA219::getType() const
{
    return MQTTSensorSensorType::INA219;
}

String Sensor_INA219::_getId(SensorTypeEnum_t type)
{
    return PrintString(F("ina219_0x%02x_%c"), _address, type);
}

void Sensor_INA219::_loop()
{
    uint32_t diff = get_time_diff(_updateTimer, millis());

    if (diff > 1000) { // reset
        _updateTimer = millis();
        _Uint = _ina219.getBusVoltage_V();
        _Iint = _ina219.getCurrent_mA();
        _Pint = _ina219.getPower_mW();
    }
    else if (diff >= IOT_SENSOR_INA219_READ_INTERVAL) {
        _updateTimer = millis();
        // calculate averages over half IN219_UPDATE_RATE period
        double num = (IN219_UPDATE_RATE * 1000 / 2) / (double)diff;
        auto current = _ina219.getCurrent_mA();
        _Uint = ((_Uint * num) + (_ina219.getBusVoltage_V())) / (num + 1);
        _Iint = ((_Iint * num) + current) / (num + 1);
        _Pint = ((_Pint * num) + (_ina219.getPower_mW())) / (num + 1);
        if (current > _Ipeak) {
            _Ipeak = current;
            _holdPeakTimer = millis();
        }
    }

    // reset peak current after IOT_SENSOR_INA219_PEAK_HOLD_TIME seconds
    if (get_time_diff(_holdPeakTimer, millis()) > IOT_SENSOR_INA219_PEAK_HOLD_TIME * 1000) {
        _holdPeakTimer = millis();
        _Ipeak = 0;
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORINA219, "SENSORINA219", "<interval in ms>", "Print INA219 sensor data");

void Sensor_INA219::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORINA219), SensorPlugin::getInstance().getName());
}

bool Sensor_INA219::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219))) {

        static EventScheduler::TimerPtr timer = nullptr;
        Scheduler.removeTimer(&timer);

        auto &serial = args.getStream();
        auto timerPrintFunc = [this, &serial](EventScheduler::TimerPtr) {
            return SensorPlugin::for_each<Sensor_INA219>(this, [&serial](Sensor_INA219 &sensor) {
                auto &ina219 = sensor.getSensor();
                serial.printf_P(PSTR("+SENSORINA219: raw: U=%d, Vshunt=%d, I=%d, current: P=%d: %.3fV, %.1fmA, %.1fmW, average: %.3fV, %.1fmA, %.1fmW\n"),
                    ina219.getBusVoltage_raw(),
                    ina219.getShuntVoltage_raw(),
                    ina219.getCurrent_raw(),
                    ina219.getPower_raw(),
                    ina219.getBusVoltage_V(),
                    ina219.getCurrent_mA(),
                    ina219.getPower_mW(),
                    sensor.getVoltage(),
                    sensor.getCurrent(),
                    sensor.getPower()
                );
            });
        };

        if (!timerPrintFunc(nullptr)) {
            serial.printf_P(PSTR("+%s: No sensor found\n"), PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219));
        }
        else {
            auto repeat = args.toMillis(AtModeArgs::FIRST, 500);
            if (repeat) {
                Scheduler.addTimer(&timer, repeat, true, timerPrintFunc);
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
