/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BATTERY

#include <ReadADC.h>
#include "Sensor_Battery.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

Sensor_Battery::Sensor_Battery(const JsonString &name) : MQTTSensor(), _name(name)
{
    REGISTER_SENSOR_CLIENT(this);
    reconfigure(nullptr);
    // read ADC every 2 seconds and store the average of 64 samples over a period of ~1.6s
    ADCManager::getInstance().addAutoReadTimer(Event::seconds(2), Event::milliseconds(50), 32);
}

Sensor_Battery::~Sensor_Battery()
{
    UNREGISTER_SENSOR_CLIENT(this);
}


MQTTComponent::MQTTAutoDiscoveryPtr Sensor_Battery::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, _getId(BatteryType::LEVEL), format);
            discovery->addStateTopic(_getTopic(BatteryType::LEVEL));
            discovery->addUnitOfMeasurement('V');
            break;
        case 1:
            discovery->create(this, _getId(BatteryType::CHARGING), format);
            discovery->addStateTopic(_getTopic(BatteryType::CHARGING));
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const {
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    return 2;
#else
    return 1;
#endif
}

void Sensor_Battery::getValues(JsonArray &array, bool timer)
{
    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(BatteryType::LEVEL));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(_readSensor(), _config.precision));

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(BatteryType::CHARGING));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _isCharging() ? FSPGM(Yes) : FSPGM(No));
#endif
}

void Sensor_Battery::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    (*row)->addSensor(_getId(BatteryType::LEVEL), _name, 'V');
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    (*row)->addSensor(_getId(BatteryType::CHARGING), F("Charging"), JsonString());
#endif
}

void Sensor_Battery::publishState(MQTTClient *client)
{
    __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (client && client->isConnected()) {
        client->publish(_getTopic(BatteryType::LEVEL), true, String(_readSensor(), _config.precision));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
        client->publish(_getTopic(BatteryType::CHARGING), true, String(_isCharging()));
#endif
    }
}

void Sensor_Battery::getStatus(Print &output)
{
    output.printf_P(PSTR("Supply Voltage Indicator"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    output.printf_P(PSTR(", charging: %s"), _isCharging() ? SPGM(Yes) : SPGM(No));
#endif
    output.printf_P(PSTR(", calibration %f" HTML_S(br)), _config.calibration);
}

bool Sensor_Battery::getSensorData(String &name, StringVector &values)
{
    name = F("Supply Voltage");
    values.emplace_back(String(_readSensor(), _config.precision) + F(" V"));
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    values.emplace_back(PrintString(F("Charging: %s"), _isCharging() ? SPGM(Yes) : SPGM(No)));
#endif
    return true;
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    String name = F("Battery Voltage");
#else
    String name = F("Supply Voltage");
#endif
    auto &group = form.addCardGroup(F("spcfg"), name, true);

    form.addPointerTriviallyCopyable(F("sp_uc"), &cfg.calibration);
    form.addFormUI(F("Calibration"));

    form.addPointerTriviallyCopyable(F("sp_ofs"), &cfg.offset);
    form.addFormUI(F("Offset"));

    form.addObjectGetterSetter(F("sp_pr"), cfg, cfg.get_bits_precision, cfg.set_bits_precision);
    form.addFormUI(F("Display Precision"));

    group.end();

#if IOT_SENSOR_BATTERY_CHARGE_DETECTION

    FormUI::Container::List pins(KFCConfigurationClasses::createFormPinList());
    FormUI::Container::List pinModes(
        Plugins::Sensor::BatteryPinMode::NONE, F("Disabled"),
        Plugins::Sensor::BatteryPinMode::ACTIVE_LOW, F("Active Low"),
        Plugins::Sensor::BatteryPinMode::ACTIVE_HIGH, F("Active High")
    );

    auto &pinsGroup = form.addCardGroup(FSPGM(config), F("Pin Configuration"), false);

    auto &pinMode0 = form.addPointerTriviallyCopyable(F("pinm0"), &cfg.pinMode[0]);
    form.addFormUI(FormUI::Type::HIDDEN);
    form.addPointerTriviallyCopyable(F("pin0"), &cfg.pins[0]);
    form.addFormUI(F("Charge Detection"), pins, FormUI::SelectSuffix(pinMode0, pinModes));

    auto &pinMode1 = form.addPointerTriviallyCopyable(F("pinm1"), &cfg.pinMode[1]);
    form.addFormUI(FormUI::Type::HIDDEN);
    form.addPointerTriviallyCopyable(F("pin1"), &cfg.pins[1]);
    form.addFormUI(F("Standby Detection"), pins, FormUI::SelectSuffix(pinMode1, pinModes));

    auto &pinMode2 = form.addPointerTriviallyCopyable(F("pinm2"), &cfg.pinMode[2]);
    form.addFormUI(FormUI::Type::HIDDEN);
    form.addPointerTriviallyCopyable(F("pin2"), &cfg.pins[2]);
    form.addFormUI(F("Running On External Power"), pins, FormUI::SelectSuffix(pinMode2, pinModes));

    auto &pinMode3 = form.addPointerTriviallyCopyable(F("pinm3"), &cfg.pinMode[3]);
    form.addFormUI(FormUI::Type::HIDDEN);
    form.addPointerTriviallyCopyable(F("pin3"), &cfg.pins[3]);
    form.addFormUI(F("Running On Battery Power"), pins, FormUI::SelectSuffix(pinMode3, pinModes));

    pinsGroup.end();

#endif

}

void Sensor_Battery::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig();
    __LDBG_printf("calibration=%f, precision=%u", _config.calibration, _config.precision);
}

// float Sensor_Battery::readSensor()
// {
//     return SensorPlugin::for_each<Sensor_Battery, float>(nullptr, NAN, [](Sensor_Battery &sensor) {
//         return sensor._readSensor();
//     });
// }

float Sensor_Battery::_readSensor()
{
    auto &adc = ADCManager::getInstance();
    auto result = adc.getAutoReadValue();
    if (result.isInvalid()) {
        __LDBG_printf("adc_get_auto_read_value is invalid");
        return NAN;
    }
    auto voltage = result.getFloatValue() / ADCManager::kMaxADCValue;

    __LDBG_printf("adc=%f raw=%f sum=%u", voltage, (((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)) / (float)IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) * voltage, sum);
    return ((((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 + IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1)) / (float)IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1) * voltage * _config.calibration) + _config.offset;
}

Sensor_Battery::SensorState Sensor_Battery::_getState() const
{
// CHARGING,
//                 STANDBY,
//                 RUNNING_ON_EXTERNAL,
//                 RUNNING_ON_BATTERY,

    if (_config.pinMode[0] && digitalRead(_config.pins[0]) == (bool)(_config.pinMode[0] - 1)) {
        return SensorState::CHARGING;
    }
    return SensorState::NONE;
}

// bool Sensor_Battery::_isCharging() const
// {
// #if IOT_SENSOR_BATTERY_CHARGE_DETECTION == -1
//     return Sensor_Battery_charging_detection();
// #else
//     return digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION);
// #endif
// }

String Sensor_Battery::_getId(BatteryType type)
{
#if IOT_SENSOR_BATTERY_CHARGE_DETECTION
    switch(type) {
        case BatteryType::CHARGING:
            return F("battery_charging");
        case BatteryType::LEVEL:
        default:
            return F("battery_level");
    }
#else
    return F("supply_voltage");
#endif
}

String Sensor_Battery::_getTopic(BatteryType type)
{
    return MQTTClient::formatTopic(_getId(type));
}

// #if AT_MODE_SUPPORTED

// #include "at_mode.h"

// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORPBV, "SENSORPBV", "<interval in ms>", "Print battery voltage");

// void Sensor_Battery::atModeHelpGenerator()
// {
//     at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(SENSORPBV), SensorPlugin::getInstance().getName_P());
// }

// bool Sensor_Battery::atModeHandler(AtModeArgs &args)
// {
//     if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORPBV))) {
//         _timer.remove();

//         auto &serial = args.getStream();
//         auto printVoltage = [&serial, this](Event::CallbackTimerPtr timer) {
//             auto value = readSensor();
//             serial.printf_P(PSTR("+SENSORPBV: %.4fV (calibration %f, offset=%f)\n"),
//                 value,
//                 _config.calibration,
//                 _config.offset
//             );
//         };
//         printVoltage(nullptr);
//         auto repeat = args.toMillis(AtModeArgs::FIRST, 500, ~0, 0, String('s'));
//         if (repeat) {
//             _Timer(_timer).add(repeat, true, printVoltage);
//         }
//         return true;
//     }
//     return false;
// }

// #endif

#endif
