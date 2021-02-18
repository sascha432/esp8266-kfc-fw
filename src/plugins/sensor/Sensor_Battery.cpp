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

enum class AutoDiscoveryNumHelperType {
    VOLTAGE = 0,
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    LEVEL,
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
    CHARGING,
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    POWER,
#endif
    MAX
};

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_Battery::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(static_cast<AutoDiscoveryNumHelperType>(num)) {
        case AutoDiscoveryNumHelperType::VOLTAGE:
            discovery->create(this, _getId(TopicType::VOLTAGE), format);
            discovery->addStateTopic(_getTopic(TopicType::VOLTAGE));
            discovery->addUnitOfMeasurement('V');
            break;
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        case AutoDiscoveryNumHelperType::LEVEL:
            discovery->create(this, _getId(TopicType::LEVEL), format);
            discovery->addStateTopic(_getTopic(TopicType::LEVEL));
            discovery->addUnitOfMeasurement('%');
            break;
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        case AutoDiscoveryNumHelperType::CHARGING:
            discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, _getId(TopicType::CHARGING), format);
            discovery->addStateTopic(_getTopic(TopicType::CHARGING));
            discovery->addPayloadOnOff();
            break;
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        case AutoDiscoveryNumHelperType::POWER:
            discovery->create(this, _getId(TopicType::POWER), format);
            discovery->addStateTopic(_getTopic(TopicType::POWER));
            break;
#endif
        case AutoDiscoveryNumHelperType::MAX:
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const
{
    return static_cast<uint8_t>(AutoDiscoveryNumHelperType::MAX);
}

void Sensor_Battery::getValues(JsonArray &array, bool timer)
{
    auto status = readSensor();

    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::VOLTAGE));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(status.getVoltage(), _config.precision));

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::LEVEL));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(status.getLevel()));
#endif

#ifdef IOT_SENSOR_BATTERY_CHARGING
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::CHARGING));
    obj->add(JJ(state), true);
    obj->add(JJ(value), status.getChargingStatus());
#endif

#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::POWER));
    obj->add(JJ(state), true);
    obj->add(JJ(value), status.getPowerStatus());
#endif
}

void Sensor_Battery::createWebUI(WebUIRoot &webUI, WebUIRow **row)
{
    (*row)->addSensor(_getId(TopicType::VOLTAGE), _name, 'V');
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    (*row)->addSensor(_getId(TopicType::LEVEL), F("Level"), '%');
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
    (*row)->addSensor(_getId(TopicType::CHARGING), F("Charging"), JsonString());
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    (*row)->addSensor(_getId(TopicType::POWER), F("Status"), JsonString());
#endif
}

void Sensor_Battery::publishState(MQTTClient *client)
{
    auto status = readSensor();
    __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (client && client->isConnected()) {
        client->publish(_getTopic(TopicType::VOLTAGE), true, String(status.getVoltage(), _config.precision));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    client->publish(_getTopic(TopicType::LEVEL), true, String(status.getLevel()));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        client->publish(_getTopic(TopicType::CHARGING), true, String(status.isCharging()));
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        client->publish(_getTopic(TopicType::POWER), true, status.getPowerStatus());
#endif
    }
}

void Sensor_Battery::getStatus(Print &output)
{
    output.printf_P(PSTR(IOT_SENSOR_NAMES_BATTERY));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    output.print(F(", Battery Level Indicator"));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
    output.print(F(", Charging Status"));
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    output.print(F(", Power Status"));
#endif
    output.printf_P(PSTR(", calibration %f" HTML_S(br)), _config.calibration);
}

bool Sensor_Battery::getSensorData(String &name, StringVector &values)
{
    auto status = readSensor();

    name = F(IOT_SENSOR_NAMES_BATTERY);
    values.emplace_back(String(status.getVoltage(), _config.precision) + F(" V"));
#ifdef IOT_SENSOR_BATTERY_CHARGING
    values.emplace_back(PrintString(F("Charging: %s"), status.isCharging() ? SPGM(Yes) : SPGM(No)));
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    values.emplace_back(PrintString(F("Status: %s"), status.getPowerStatus()));
#endif
    return true;
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("spcfg"), F(IOT_SENSOR_NAMES_BATTERY), true);

    form.addPointerTriviallyCopyable(F("sp_uc"), &cfg.calibration);
    form.addFormUI(F("Calibration"));

    form.addPointerTriviallyCopyable(F("sp_ofs"), &cfg.offset);
    form.addFormUI(F("Offset"));

    form.addObjectGetterSetter(F("sp_pr"), cfg, cfg.get_bits_precision, cfg.set_bits_precision);
    form.addFormUI(F("Display Precision"));
    cfg.addRangeValidatorFor_precision(form);

    group.end();
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

void Sensor_Battery::Status::readSensor(Sensor_Battery &sensor)
{
    auto &adc = ADCManager::getInstance();
    auto result = adc.getAutoReadValue(true);
    float adcVal = result.getMeanValue();
    auto Vout = ((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * adcVal) + (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 * adcVal)) / (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * ADCManager::kMaxADCValue);
    _voltage = Vout * sensor._config.calibration + sensor._config.offset;

#if DEBUG
    result.dump(DEBUG_OUTPUT);

    __DBG_printf("adc=%u adcval=%f Vout=%f Vcal=%f",
        system_adc_read(),
        adcVal,
        Vout,
        _voltage
    );
#endif

#ifdef IOT_SENSOR_BATTERY_CHARGING
#ifdef IOT_SENSOR_BATTERY_CHARGING_COMPLETE
    if (IOT_SENSOR_BATTERY_CHARGING_COMPLETE) {
        _charging = ChargingType::COMPLETE;
    }
    else
#endif
    {
        _charging = IOT_SENSOR_BATTERY_CHARGING ? ChargingType::CHARGING : ChargingType::NONE;
    }
#endif

#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    _state = IOT_SENSOR_BATTERY_ON_EXTERNAL ? StateType::RUNNING_ON_EXTERNAL_POWER : StateType::RUNNING;
#endif


}

Sensor_Battery::Status Sensor_Battery::readSensor()
{
    if (!_status.isValid()) {
        _status.readSensor(*this);
    }
    return _status;
}


// bool Sensor_Battery::_isCharging() const
// {
// #if IOT_SENSOR_BATTERY_CHARGING_HAVE_FUNCTION
//     return IOT_SENSOR_BATTERY_CHARGING_FUNCTION;
// #elif IOT_SENSOR_BATTERY_CHARGING_PIN == -1
//     return false;
// #else
//     return digitalRead(IOT_SENSOR_BATTERY_CHARGING_PIN);
// #endif
// }

// bool Sensor_Battery::_isOnExternalPower() const
// {
// #if IOT_SENSOR_BATTERY_ON_EXTERNAL_PIN == -1
//     return true;
// #else
//     return digitalRead(IOT_SENSOR_BATTERY_ON_EXTERNAL_PIN);
// #endif
// }

String Sensor_Battery::_getId(TopicType type)
{
    switch(type) {
        case TopicType::VOLTAGE:
            return F("voltage");
        case TopicType::POWER:
            return F("power");
        case TopicType::CHARGING:
            return F("charging");
        case TopicType::LEVEL:
        default:
            return F("level");
    }
}

String Sensor_Battery::_getTopic(TopicType type)
{
    return MQTTClient::formatTopic(_getId(type));
}

static float __scale(float value)
{
    float factor = (value - Sensor_Battery::kLipoMinV) / (Sensor_Battery::kLipoMaxV - Sensor_Battery::kLipoMinV);
    return (Sensor_Battery::kLipoMaxV - Sensor_Battery::kRegressMin) * factor + Sensor_Battery::kRegressMin;
}

static float __regress(float x) {
    static constexpr auto terms = std::array<float, 4>({
        4.1122660195235891e+004,
        -3.2838992834749057e+004,
        8.6906274643468823e+003,
        -7.6139591374792838e+002
    });
    float t = 1;
    float r = 0;
    for(auto term: terms) {
        r += term * t;
        t *= x;
    }
    return r;
}

uint8_t Sensor_Battery::calcLipoCapacity(float voltage, uint8_t cells, float discharge)
{
    voltage /= cells; // per cell voltage
    voltage *= std::min<float>(1, (1 - (discharge * 5 / 100.0))); // add an artifical voltage drop
    if (voltage <= kLipoMinV) {
        return 0;
    }
    if (voltage >= kLipoMaxV) {
        return 100;
    }
    if (voltage >= kRegressMax) { // show linear drop for the range that isn't covered
        return 100 - std::max<uint8_t>(0, (kLipoMaxV - voltage) / voltage) * 100;
    }
    voltage = __scale(voltage); // scale voltage to fit regress function
    if (voltage <= kRegressMin) {
        return 0;
    }
    return __regress(voltage);
}

#endif
