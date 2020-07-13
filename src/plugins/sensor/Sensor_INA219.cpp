/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_INA219

#include <Arduino_compat.h>
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

Sensor_INA219::Sensor_INA219(const JsonString &name, TwoWire &wire, uint8_t address) : MQTTSensor(), _name(name), _address(address), _updateTimer(0), _holdPeakTimer(0), _Ipeak(NAN), _ina219(address)
{
    REGISTER_SENSOR_CLIENT(this);
    _ina219.begin(&config.initTwoWire());
    _ina219.setCalibration(IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES, IOT_SENSOR_INA219_R_SHUNT * 4);

    _debug_printf_P(PSTR("Sensor_INA219::Sensor_INA219(): address=%x, voltage range=%x, gain=%x, shunt ADC resolution=%x\n"), _address, IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES);

    setUpdateRate(IN219_WEBUI_UPDATE_RATE);
    LoopFunctions::add([this]() {
        this->_loop();
    }, reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

Sensor_INA219::~Sensor_INA219()
{
    LoopFunctions::remove(reinterpret_cast<LoopFunctions::CallbackPtr_t>(this));
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_INA219::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, _getId(VOLTAGE), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId(VOLTAGE)));
            discovery->addUnitOfMeasurement('V');
            break;
        case 1:
            discovery->create(this, _getId(CURRENT), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId(CURRENT)));
            discovery->addUnitOfMeasurement(F("mA"));
            break;
        case 2:
            discovery->create(this, _getId(POWER), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId(POWER)));
            discovery->addUnitOfMeasurement(F("mW"));
            break;
        case 3:
            discovery->create(this, _getId(PEAK_CURRENT), format);
            discovery->addStateTopic(MQTTClient::formatTopic(_getId(PEAK_CURRENT)));
            discovery->addUnitOfMeasurement(F("mA"));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_INA219::getAutoDiscoveryCount() const
{
    return 4;
}

void Sensor_INA219::getValues(JsonArray &array, bool timer)
{
    _debug_printf_P(PSTR("Sensor_INA219::getValues()\n"));
    auto *obj = &array.addObject(3);
    obj->add(JJ(id), _getId(VOLTAGE));
    auto U = _data.U();
    obj->add(JJ(state), !isnan(U));
    obj->add(JJ(value), JsonNumber(U, 2));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(CURRENT));
    auto I = _data.I();
    obj->add(JJ(state), !isnan(I));
    obj->add(JJ(value), JsonNumber(I, 0));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(POWER));
    auto P = _data.P();
    obj->add(JJ(state), !isnan(P));
    obj->add(JJ(value), JsonNumber(P, 0));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(PEAK_CURRENT));
    obj->add(JJ(state), !isnan(_Ipeak));
    obj->add(JJ(value), JsonNumber(_Ipeak, 0));

    if (timer) {
        _data = SensorData();
    }
}

void Sensor_INA219::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_INA219::createWebUI()\n"));
    (*row)->addSensor(_getId(VOLTAGE), _name, 'V');
    (*row)->addSensor(_getId(CURRENT), F("Current"), F("mA"));
    (*row)->addSensor(_getId(POWER), F("Power"), F("mW"));
    (*row)->addSensor(_getId(PEAK_CURRENT), F("Peak Current"), F("mA"));
}

void Sensor_INA219::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        auto _qos = MQTTClient::getDefaultQos();
        client->publish(MQTTClient::formatTopic(_getId(VOLTAGE)), _qos, true, String(_mqttData.U(), 2));
        client->publish(MQTTClient::formatTopic(_getId(CURRENT)), _qos, true, String(_mqttData.I(), 0));
        client->publish(MQTTClient::formatTopic(_getId(POWER)), _qos, true, String(_mqttData.P(), 0));
        client->publish(MQTTClient::formatTopic(_getId(PEAK_CURRENT)), _qos, true, String(_Ipeak, 0));
        _mqttData = SensorData();
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
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORINA219), SensorPlugin::getInstance().getName());
}

bool Sensor_INA219::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219))) {

        static EventScheduler::Timer timer;
        timer.remove();

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
                    ina219.getBusVoltage_V() * ina219.getCurrent_mA(),
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
            auto repeat = args.toMillis(AtModeArgs::FIRST, 500, ~0, 0, String('s'));
            if (repeat) {
                timer.add(repeat, true, timerPrintFunc);
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
