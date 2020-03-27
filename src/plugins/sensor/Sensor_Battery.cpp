/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_BATTERY

#include "Sensor_Battery.h"
#include "progmem_data.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_Battery::Sensor_Battery(const JsonString &name) : MQTTSensor(), _name(name), _adc(A0, 20, 1)
{
    REGISTER_SENSOR_CLIENT(this);
    reconfigure();
}

void Sensor_Battery::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_getTopic(LEVEL));
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(discovery);

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    discovery = new MQTTAutoDiscovery();
    discovery->create(this, 1, format);
    discovery->addStateTopic(_getTopic(STATE));
    discovery->finalize();
    vector.emplace_back(discovery);
#endif
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const {
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    return 2;
#else
    return 1;
#endif
}

void Sensor_Battery::getValues(JsonArray &array)
{
    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(LEVEL));
    obj->add(JJ(state), true);
    obj->add(JJ(value), String(_readSensor(), _config.precision));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(STATE));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _isCharging() ? FSPGM(Yes) : FSPGM(No));
#endif
}

void Sensor_Battery::createWebUI(WebUI &webUI, WebUIRow **row)
{
    (*row)->addSensor(_getId(LEVEL), _name, F("V"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    (*row)->addSensor(_getId(STATE), F("Charging"), F(""));
#endif
}

void Sensor_Battery::publishState(MQTTClient *client)
{
    _debug_printf_P(PSTR("client=%p connected=%u\n"), client, client && client->isConnected() ? 1 : 0);
    if (client && client->isConnected()) {
        client->publish(_getTopic(LEVEL), _qos, 1, String(_readSensor(), _config.precision));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
        client->publish(_getTopic(STATE), _qos, 1, _isCharging() ? FSPGM(Yes) : FSPGM(No));
#endif
    }
}

void Sensor_Battery::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("Supply Voltage Indicator"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    output.printf_P(PSTR(", charging: %s"), _isCharging() ? SPGM(Yes) : SPGM(No));
#endif
    output.printf_P(PSTR(", calibration %f" HTML_S(br)),  _config.calibration);
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.add<float>(F("battery_calibration"), _H_STRUCT_VALUE(Config().sensor, battery.calibration))->setFormUI(new FormUI(FormUI::TEXT, F("Supply Voltage Calibration")));
    form.add<float>(F("battery_offset"), _H_STRUCT_VALUE(Config().sensor, battery.offset))->setFormUI(new FormUI(FormUI::TEXT, F("Supply Voltage Offset")));
    form.add<uint8_t>(F("battery_precision"), _H_STRUCT_VALUE(Config().sensor, battery.precision))->setFormUI(new FormUI(FormUI::TEXT, F("Supply Voltage Precision")));
}

void Sensor_Battery::reconfigure()
{
    _config = config._H_GET(Config().sensor).battery;
    _debug_printf_P(PSTR("calibration=%f, precision=%u\n"), _config.calibration, _config.precision);
}


float Sensor_Battery::readSensor()
{
    return SensorPlugin::for_each<Sensor_Battery, float>(nullptr, NAN, [](Sensor_Battery &sensor) {
        return sensor._readSensor();
    });
}

float Sensor_Battery::_readSensor()
{
    double adcVoltage = _adc.read() / 1024.0;
    _debug_printf_P(PSTR("raw = %f\n"), (((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)) / IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) * adcVoltage)
    return ((((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)) / IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) * adcVoltage * _config.calibration) + _config.offset;
}

bool Sensor_Battery::_isCharging() const
{
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION == -1
    return Sensor_Battery_charging_detection();
#else
    return digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION);
#endif
}

String Sensor_Battery::_getId(BatteryIdEnum_t type)
{
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    switch(type) {
        case BatteryIdEnum_t::STATE:
            return F("battery_state");
        case BatteryIdEnum_t::LEVEL:
        default:
            return F("battery_level");
    }
#else
    return F("supply_voltage");
#endif
}

String Sensor_Battery::_getTopic(BatteryIdEnum_t type)
{
    return MQTTClient::formatTopic(-1, F("/%s/"), _getId(type).c_str());
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORPBV, "SENSORPBV", "<interval in ms>", "Print battery voltage");

void Sensor_Battery::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORPBV), SensorPlugin::getInstance().getName());
}

bool Sensor_Battery::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORPBV))) {
        _timer.remove();

        auto serial = &args.getStream();
        auto printVoltage = [serial](EventScheduler::TimerPtr timer) {
            auto value = Sensor_Battery::readSensor();
            serial->printf_P(PSTR("+SENSORPBV: %.4fV (calibration %f, offset=%f)\n"),
                value,
                config._H_GET(Config().sensor).battery.calibration,
                config._H_GET(Config().sensor).battery.offset
            );
        };
        printVoltage(nullptr);
        auto repeat = args.toMillis(AtModeArgs::FIRST, 500, ~0, 0, String('s'));
        if (repeat) {
            _timer.add(repeat, true, printVoltage);
        }
        return true;
    }
    return false;
}

#endif

#endif
