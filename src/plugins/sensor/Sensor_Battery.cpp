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

float Sensor_Battery::maxVoltage = 0;

using KFCConfigurationClasses::System;
using SensorRecordType = Plugins::Sensor::SensorRecordType;

uint8_t operator&(SensorRecordType a, SensorRecordType b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

uint8_t operator&(uint8_t a, SensorRecordType b) {
    return a & static_cast<uint8_t>(b);
}

static constexpr uint32_t kReadInterval = 125;
static constexpr uint32_t kUpdateInterval = 2000;

Sensor_Battery::Sensor_Battery(const JsonString &name) : MQTTSensor(), _name(name), _adcValue(NAN), _adcLastUpdateTime(1U << 31), _timerCounter(0)
{
    REGISTER_SENSOR_CLIENT(this);
    reconfigure(nullptr);

    _readADC(true);
    _adcValue = NAN; // discard initial reading

    _Timer(_timer).add(Event::milliseconds(kReadInterval), true, [this](Event::CallbackTimerPtr) {
        if (++_timerCounter >= kUpdateInterval / kReadInterval) {
            _timerCounter = 0;
        }
        _readADC(_timerCounter == 0);
    });
}

Sensor_Battery::~Sensor_Battery()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

void Sensor_Battery::_readADC(bool updateSensor)
{
    auto value = ADCManager::getInstance().readValue();
    auto _millis = millis();
    uint32_t diff;
    if (isnan(_adcValue)) {
        diff = 0;
        _adcValue = value;
    }
    else {
        diff = get_time_diff(_adcLastUpdateTime, _millis);
        float f = (20 * 1000) / (float)diff;
        _adcValue = ((_adcValue * f) + value) / (f + 1.0);
    }
    _adcLastUpdateTime = _millis;

    if (updateSensor) {
        _status.updateSensor(*this);
    }

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
    if (diff && (_config.record & SensorRecordType::ADC) && _config.address && _config.port && WiFi.isConnected()) {
        WiFiUDP udp;
        if (udp.beginPacket(IPAddress(_config.address), _config.port) &&
            udp.printf_P(PSTR("[\"%s\",\"ADC\",%.3f,%u,%u]"), System::Device::getName(), static_cast<float>(time(nullptr)) + ((millis() % 1000) / 1000.0), value, diff) &&
            udp.endPacket())
        {
            // success
        }
    }
#endif

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

Sensor_Battery::AutoDiscoveryPtr Sensor_Battery::nextAutoDiscovery(FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = __LDBG_new(AutoDiscovery);
    switch(static_cast<AutoDiscoveryNumHelperType>(num)) {
        case AutoDiscoveryNumHelperType::VOLTAGE:
            discovery->create(this, _getId(TopicType::VOLTAGE), format);
            discovery->addStateTopic(_getTopic(TopicType::VOLTAGE));
            discovery->addUnitOfMeasurement('V');
            discovery->addDeviceClass(F("voltage"));
            break;
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        case AutoDiscoveryNumHelperType::LEVEL:
            discovery->create(this, _getId(TopicType::LEVEL), format);
            discovery->addStateTopic(_getTopic(TopicType::LEVEL));
            discovery->addUnitOfMeasurement('%');
            discovery->addDeviceClass(F("battery"));
            break;
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        case AutoDiscoveryNumHelperType::CHARGING:
            discovery->create(ComponentType::SENSOR, _getId(TopicType::CHARGING), format);
            discovery->addStateTopic(_getTopic(TopicType::CHARGING));
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
    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::VOLTAGE));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(_status.getVoltage(), _config.precision));

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::LEVEL));
    obj->add(JJ(state), true);
    obj->add(JJ(value), JsonNumber(_status.getLevel()));
#endif

#ifdef IOT_SENSOR_BATTERY_CHARGING
    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(TopicType::CHARGING));
    obj->add(JJ(state), true);
    obj->add(JJ(value), _status.getChargingStatus());
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
    // auto status = readSensor();
    // if (!_status.isValid()) {
    //     return;
    // }
    __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (client && client->isConnected()) {
        client->publish(_getTopic(TopicType::VOLTAGE), true, String(_status.getVoltage(), _config.precision));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    client->publish(_getTopic(TopicType::LEVEL), true, String(_status.getLevel()));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        client->publish(_getTopic(TopicType::CHARGING), true, _status.getChargingStatus());
#endif
#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
        client->publish(_getTopic(TopicType::POWER), true, _status.getPowerStatus());
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
    form.addFormUI(F("Calibration"), FormUI::Suffix(PrintString(F("Max. Voltage %.4f"), maxVoltage)));

    form.addPointerTriviallyCopyable(F("sp_ofs"), &cfg.offset);
    form.addFormUI(F("Offset"));

    form.addObjectGetterSetter(F("sp_pr"), cfg, cfg.get_bits_precision, cfg.set_bits_precision);
    form.addFormUI(F("Display Precision"));
    cfg.addRangeValidatorFor_precision(form);

#if IOT_SENSOR_HAVE_BATTERY_RECORDER

    form.addObjectGetterSetter(F("sp_ra"), cfg, cfg.get_ipv4_address, cfg.set_ipv4_address);
    form.addFormUI(F("Data Hostname"));

    form.addObjectGetterSetter(F("sp_rp"), cfg, cfg.get_bits_port, cfg.set_bits_port);
    form.addFormUI(F("Data Port"));
    cfg.addRangeValidatorFor_port(form);

    auto items = FormUI::List(
        SensorRecordType::NONE, F("Disabled"),
        SensorRecordType::ADC, F("Record ADC values"),
        SensorRecordType::SENSOR, F("Record sensor values"),
        SensorRecordType::BOTH, F("Record ADC and sensor values")
    );

    form.addObjectGetterSetter(F("sp_brt"), cfg, cfg.get_int_record, cfg.set_int_record);
    form.addFormUI(F("Sensor Data"), items);

#endif

    group.end();
}

void Sensor_Battery::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig();
    maxVoltage = 0;
}

// float Sensor_Battery::readSensor()
// {
//     return SensorPlugin::for_each<Sensor_Battery, float>(nullptr, NAN, [](Sensor_Battery &sensor) {
//         return sensor._readSensor();
//     });
// }

void Sensor_Battery::Status::updateSensor(Sensor_Battery &sensor)
{
    float Vout = ((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * sensor._adcValue) + (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 * sensor._adcValue)) / (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * ADCManager::kMaxADCValue);
    maxVoltage = std::max(maxVoltage, Vout);
    _voltage = Vout * sensor._config.calibration + sensor._config.offset;

#ifdef IOT_SENSOR_BATTERY_CHARGING
#if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN != -1
    if (IOT_SENSOR_BATTERY_CHARGING) {
        noInterrupts();
        pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT_PULLUP);
        auto value = digitalRead(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN);
        pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT);
        interrupts();
        _charging = value ? ChargingType::CHARGING : ChargingType::COMPLETE;
    }
    else {
        _charging = ChargingType::NONE;
    }
#else
    _charging = (IOT_SENSOR_BATTERY_CHARGING) ? ChargingType::CHARGING : ChargingType::NONE;
#endif
#endif

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    _level = IOT_SENSOR_BATTERY_LEVEL_FUNCTION(_voltage,  IOT_SENSOR_BATTERY_NUM_CELLS, _charging == ChargingType::CHARGING);
#endif
    // // make sure that the level decreases only when not charging and increases while charging
    // if (_charging != _pCharging) {
    //     _pCharging = _charging;
    //     if (_charging == ChargingType::CHARGING) {
    //         _level = 0;
    //     }
    //     else {
    //         // _level = 100;
    //     }
    // }

    // _level = IOT_SENSOR_BATTERY_LEVEL_FUNCTION(_voltage,  IOT_SENSOR_BATTERY_NUM_CELLS, _charging == ChargingType::CHARGING);
    // if (_charging == ChargingType::CHARGING) {
    //     _level = std::max(_level, level);
    // }
    // else {
    //     _level = level;
    //     // _level = std::min(_level, level);
    // }
// #endif


#if IOT_SENSOR_BATTERY_DSIPLAY_POWER_STATUS
    _state = IOT_SENSOR_BATTERY_ON_EXTERNAL ? StateType::RUNNING_ON_EXTERNAL_POWER : StateType::RUNNING;
#endif

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
    if ((sensor._config.record & SensorRecordType::SENSOR) && sensor._config.address && sensor._config.port && WiFi.isConnected()) {
        WiFiUDP udp;
        if (udp.beginPacket(IPAddress(sensor._config.address), sensor._config.port) &&
            udp.printf_P(PSTR("[\"%s\",\"SEN\",%.3f,%.4f,%.4f,%.4f,%u,%u]"),
                KFCConfigurationClasses::System::Device::getName(),
                static_cast<float>(time(nullptr)) + ((millis() % 1000) / 1000.0),
                _voltage, Vout, sensor._adcValue, _level, _charging == ChargingType::CHARGING) &&
            udp.endPacket())
        {
            // sent
        }
    }
#endif

}

Sensor_Battery::Status Sensor_Battery::readSensor()
{
    return _status;
}

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


static float __regress(float x) {
    static constexpr auto terms = std::array<float, 8>({
        3.0712731081353914e+000,
        1.2253735352323229e+001,
        -9.3462148496129430e+001,
        3.8661734533561298e+002,
        -8.9481613881130249e+002,
        1.1585379334963445e+003,
        -7.8168640078108967e+002,
        2.1366643258750511e+002
    });
    float t = 1;
    float r = 0;
    for(auto term: terms) {
        r += term * t;
        t *= x;
    }
    return r;
}

static float __regress_charging(float x) {
    static constexpr auto terms = std::array<float, 11>({
        3.2160580311842994e+000,
        2.1758288145016344e+001,
        -3.2813311670421581e+002,
        2.7580268690692337e+003,
        -1.3643884107267739e+004,
        4.1846841437876174e+004,
        -8.1750566367533160e+004,
        1.0180616101308420e+005,
        -7.8222156393968166e+004,
        3.3791053129104359e+004,
        -6.2780878948850805e+003
    });
    float t = 1;
    float r = 0;
    for(auto term: terms) {
        r += term * t;
        t *= x;
    }
    return r;
}


uint8_t Sensor_Battery::calcLipoCapacity(float voltage, uint8_t cells, bool charging)
{
    RegressFunction func = charging ? __regress_charging : __regress;
    voltage /= cells;

    using IntType = uint8_t;
    IntType first = 0;
    IntType last = 100;
    IntType count = last;
    while (count > 0) {
        IntType it = first;
        IntType step = count / 2;
        it += step;
        if (func(it / 100.0f) < voltage) {
            first = it + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return first;
}

#if AT_MODE_SUPPORTED && (IOT_SENSOR_BATTERY_DISPLAY_LEVEL || IOT_SENSOR_HAVE_BATTERY_RECORDER)

#include "at_mode.h"

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCAP, "BCAP", "<voltage>", "Calculate battery capacity for given voltage");
#endif
#if IOT_SENSOR_HAVE_BATTERY_RECORDER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BREC, "BREC", "[<host=192.168.0.3>,<port=19523>,<type>]", "Enable sensor data recording");
#endif

void Sensor_Battery::atModeHelpGenerator()
{
    auto name = SensorPlugin::getInstance().getName_P();
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(BCAP), name);
#endif
#if IOT_SENSOR_HAVE_BATTERY_RECORDER
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(BREC), name);
#endif
}

bool Sensor_Battery::atModeHandler(AtModeArgs &args)
{
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCAP))) {
        auto voltage = args.toFloat(0);
        float capacity = Sensor_Battery::calcLipoCapacity(voltage, 1, false);
        float charging = Sensor_Battery::calcLipoCapacity(voltage, 1, true);
        args.printf_P(PSTR("voltage=%.4fV capacity=%.1f%% charging=%.1f%% Umax=%.4f"), voltage, capacity, charging, Sensor_Battery::maxVoltage);
        return true;
    }
#endif
#if IOT_SENSOR_HAVE_BATTERY_RECORDER
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BREC))) {
        if (args.size() >= 2) {
            auto address = IPAddress();
            address.fromString(args.toString(0));
            _config.address = static_cast<uint32_t>(address);
            _config.port = args.toIntMinMax<uint16_t>(1, 1, 65535, 19523);
            _config.record = args.toIntMinMax<uint8_t>(2, 0, 3);
            config.write();
            args.printf_P(PSTR("sending sensor data to %s:%u UDP type=%u"), address.toString().c_str(), _config.port, _config.record);
        }
        else {
            _config.record = 0;
            config.write();
            args.print(F("battery voltage recording disabled"));
        }
        return true;
    }
#endif
    return false;
}

#endif

#endif
