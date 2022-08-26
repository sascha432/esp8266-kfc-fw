/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_BATTERY

#include <ReadADC.h>
#include <stl_ext/array.h>
#include "Sensor_Battery.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

float Sensor_Battery::maxVoltage = 0;

using KFCConfigurationClasses::System;
using SensorRecordType = KFCConfigurationClasses::Plugins::SensorConfigNS::SensorRecordType;

uint8_t operator&(SensorRecordType a, SensorRecordType b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

uint8_t operator&(uint8_t a, SensorRecordType b) {
    return a & static_cast<uint8_t>(b);
}

static constexpr uint32_t kReadInterval = 125;
static constexpr uint32_t kUpdateInterval = 2000;

Sensor_Battery::Sensor_Battery(const String &name) :
    MQTT::Sensor(MQTT::SensorType::BATTERY),
    _name(name),
    _adcValue(NAN),
    _adcLastUpdateTime(1U << 31),
    _timerCounter(std::max<int>(0, (kUpdateInterval - 600) / kReadInterval))
{
    REGISTER_SENSOR_CLIENT(this);
    reconfigure(nullptr);

    ADCManager::getInstance().addAutoReadTimer(Event::seconds(1), Event::milliseconds(100), 10);

    _Timer(_timer).add(Event::milliseconds(kReadInterval), true, [this](Event::CallbackTimerPtr) {
        // the sensor will be updated at kUpdateInterval and the first time ~600ms after the timer has been started
        _readADC((++_timerCounter % (kUpdateInterval / kReadInterval)) == 0);
    });
}

Sensor_Battery::~Sensor_Battery()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

void Sensor_Battery::_readADC(bool updateSensor)
{
    auto value = ADCManager::getInstance().readValue();
    if (value >= ADCManager::kMaxADCValue - 10) {
        // skip unusual high values
        __DBG_printf_N("ADC value %u", value);
        return;
    }
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
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
    POWER,
#endif
    MAX
};

MQTT::AutoDiscovery::EntityPtr Sensor_Battery::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(static_cast<AutoDiscoveryNumHelperType>(num)) {
        case AutoDiscoveryNumHelperType::VOLTAGE:
            if (discovery->create(this, _getId(TopicType::VOLTAGE), format)) {
                discovery->addStateTopic(_getTopic(TopicType::VOLTAGE));
                // discovery->addUnitOfMeasurement('V');
                discovery->addDeviceClass(F("voltage"), 'V');
                discovery->addName(_name);
                discovery->addObjectId(baseTopic + MQTT::Client::filterString(_name, true));
            }
            break;
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        case AutoDiscoveryNumHelperType::LEVEL:
            if (discovery->create(this, _getId(TopicType::LEVEL), format)) {
                discovery->addStateTopic(_getTopic(TopicType::LEVEL));
                // discovery->addUnitOfMeasurement('%');
                discovery->addDeviceClass(F("battery"), '%');
                discovery->addName(F("Battery Level"));
                discovery->addObjectId(baseTopic + F("battery_level"));
            }
            break;
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        case AutoDiscoveryNumHelperType::CHARGING:
            if (discovery->create(ComponentType::SENSOR, _getId(TopicType::CHARGING), format)) {
                discovery->addStateTopic(_getTopic(TopicType::CHARGING));
                discovery->addName(F("Charging Status"));
                discovery->addObjectId(baseTopic + F("charging_status"));
            }
            break;
#endif
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
        case AutoDiscoveryNumHelperType::POWER:
            if (discovery->create(this, _getId(TopicType::POWER), format)) {
                discovery->addStateTopic(_getTopic(TopicType::POWER));
                discovery->addName(F("Power Status"));
                discovery->addObjectId(baseTopic + F("power_status"));
            }
            break;
#endif
        case AutoDiscoveryNumHelperType::MAX:
            break;
    }
    return discovery;
}

uint8_t Sensor_Battery::getAutoDiscoveryCount() const
{
    return static_cast<uint8_t>(AutoDiscoveryNumHelperType::MAX);
}

void Sensor_Battery::getValues(WebUINS::Events &array, bool timer)
{
    using namespace WebUINS;
    array.append(
        Values(_getId(TopicType::VOLTAGE), TrimmedFloat(_status.getVoltage(), _config.precision))
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        , Values(_getId(TopicType::LEVEL), _status.getLevel(), true)
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        , Values(_getId(TopicType::CHARGING), _status.getChargingStatus(), true)
#endif
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
        , Values(_getId(TopicType::POWER), _status.getPowerStatus(), true)
        )
#endif
    );
}

void Sensor_Battery::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(
        WebUINS::Sensor(_getId(TopicType::VOLTAGE), _name, 'V').setConfig(_renderConfig)
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        , WebUINS::Sensor(_getId(TopicType::LEVEL), F("Level"), '%').setConfig(_renderConfig)
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        , WebUINS::Sensor(_getId(TopicType::CHARGING), F("Charging"), F("")).setConfig(_renderConfig)
#endif
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
        , WebUINS::Sensor(_getId(TopicType::POWER), F("Status"), F("")).setConfig(_renderConfig)
#endif
    );
    webUI.appendToLastRow(row);
}

void Sensor_Battery::publishState()
{
    // auto status = readSensor();
    // if (!_status.isValid()) {
    //     return;
    // }
    __LDBG_printf("client=%p connected=%u", client, client && client->isConnected() ? 1 : 0);
    if (isConnected()) {
        publish(_getTopic(TopicType::VOLTAGE), true, String(_status.getVoltage(), _config.precision));
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        publish(_getTopic(TopicType::LEVEL), true, String(_status.getLevel()));
#endif
#ifdef IOT_SENSOR_BATTERY_CHARGING
        publish(_getTopic(TopicType::CHARGING), true, _status.getChargingStatus());
#endif
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
        publish(_getTopic(TopicType::POWER), true, _status.getPowerStatus());
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
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
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
#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
    values.emplace_back(PrintString(F("Status: %s"), status.getPowerStatus()));
#endif
    return true;
}

void Sensor_Battery::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("spcfg"), F(IOT_SENSOR_NAMES_BATTERY), true);

    form.addPointerTriviallyCopyable<float>(F("sp_uc"), reinterpret_cast<void *>(&cfg.battery.calibration));
    form.addFormUI(F("Calibration"), FormUI::Suffix(PrintString(F("Max. Voltage %.4f"), maxVoltage)));
    cfg.battery.addRangeValidatorFor_calibration(form);

    form.addPointerTriviallyCopyable<float>(F("sp_ofs"), reinterpret_cast<void *>(&cfg.battery.offset));
    form.addFormUI(F("Offset"));
    cfg.battery.addRangeValidatorFor_offset(form);

    form.addObjectGetterSetter(F("sp_pr"),  FormGetterSetter(cfg.battery, precision));
    form.addFormUI(F("Display Precision"));
    cfg.battery.addRangeValidatorFor_precision(form);

#if IOT_SENSOR_HAVE_BATTERY_RECORDER

    form.addObjectGetterSetter(F("sp_ra"), FormGetterSetter(cfg.battery, address));
    form.addFormUI(F("Data Hostname"));
    cfg.battery.addHostnameValidatorFor_address(form)

    form.addObjectGetterSetter(F("sp_rp"), FormGetterSetter(cfg.battery, port));
    form.addFormUI(F("Data Port"));
    cfg.addRangeValidatorFor_port(form);

    auto items = FormUI::List(
        SensorRecordType::NONE, F("Disabled"),
        SensorRecordType::ADC, F("Record ADC values"),
        SensorRecordType::SENSOR, F("Record sensor values"),
        SensorRecordType::BOTH, F("Record ADC and sensor values")
    );

    form.addObjectGetterSetter(F("sp_brt"), FormGetterSetter(cfg.battery, record));
    form.addFormUI(F("Sensor Data"), items);

#endif

    group.end();
}

void Sensor_Battery::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().battery;
    maxVoltage = 0;
}

void Sensor_Battery::Status::updateSensor(Sensor_Battery &sensor)
{
    if (isnan(sensor._adcValue) || sensor._timerCounter < 5) {
        // no (enough) data available yet
        return;
    }
    // do not update the voltage when the ADC is at maximum
    // could be a bug or ADC issue with deep sleep
    float Vout = ((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * sensor._adcValue) + (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 * sensor._adcValue)) / (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * ADCManager::kMaxADCValue);
    maxVoltage = std::max(maxVoltage, Vout);
    _voltage = Vout * sensor._config.calibration + sensor._config.offset;

#ifdef IOT_SENSOR_BATTERY_CHARGING
#if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN != -1
    if (IOT_SENSOR_BATTERY_CHARGING) {
#if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN == 3
        noInterrupts();
        // KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE, SERIAL_8N1, SERIAL_TX_ONLY);
        pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT_PULLUP);
#endif
        auto value = digitalRead(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN);
#if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN == 3
        pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT);
        // KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE);
        interrupts();
#endif
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
    // do not update the level for 20 seconds if the charging state changes
    // it needs some time to settle or the values will be quite different
    // do not lock the level until 5 seconds after the reboot
    if (_chargingBefore != _charging) {
        _chargingBefore = _charging;
        auto ms = millis();
        if (ms > 5000) {
            _lockLevelTime = ms + 20000;
        }
    }
    if (_lockLevelTime == 0) {
        _level = IOT_SENSOR_BATTERY_LEVEL_FUNCTION(_voltage,  IOT_SENSOR_BATTERY_NUM_CELLS, _charging == ChargingType::CHARGING);
    }
    else if (millis() > _lockLevelTime) {
        _lockLevelTime = 0;
    }
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


#if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
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


inline static float __regress(float x) {
    static constexpr auto terms PROGMEM = stdex::array_of<const float>(
        3.0901165359671228e+000,
        1.3918435736462101e-001,
        -1.4396426399182059e-002,
        8.7076685354720170e-004,
        -3.1845853329535204e-005,
        7.2574155135737268e-007,
        -1.0360837215997591e-008,
        9.0101915491340791e-011,
        -4.3670186360225468e-013,
        9.0520507287591813e-016
    );
    float t = 1;
    float r = 0;
    for(auto term: terms) {
        r += term * t;
        t *= x;
    }
    return r;
}

inline static float __regress_charging(float x) {
    static constexpr auto terms PROGMEM = stdex::array_of<const float>(
        3.2930194940793633e+000,
        2.0635303243176276e-001,
        -3.5146285982269383e-002,
        3.4103882409114760e-003,
        -1.9841294648274642e-004,
        7.3054359786938261e-006,
        -1.7598705546951781e-007,
        2.8107121828560341e-009,
        -2.9502555631192765e-011,
        1.9567430256134823e-013,
        -7.4374991457700413e-016,
        1.2350426541328422e-018
    );
    float t = 1;
    float r = 0;
    for(auto term: terms) {
        r += term * t;
        t *= x;
    }
    return r;
}


float Sensor_Battery::calcLipoCapacity(float voltage, uint8_t cells, bool charging, float precision)
{
    RegressFunction func = charging ? __regress_charging : __regress;
    voltage /= cells;

    using IntType = uint16_t;
    IntType first = 0;
    IntType last = 100 * precision;
    IntType count = last;
    while (count > 0) {
        IntType it = first;
        IntType step = count / 2;
        it += step;
        if (func(it / precision) < voltage) {
            first = it + 1;
            count -= step + 1;
        }
        else {
            count = step;
        }
    }
    return first / precision;
}

#if AT_MODE_SUPPORTED && (IOT_SENSOR_BATTERY_DISPLAY_LEVEL || IOT_SENSOR_HAVE_BATTERY_RECORDER)

#include "at_mode.h"

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCAP, "BCAP", "<voltage>[,<precison=1-10>]", "Calculate battery capacity for given voltage");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCTAB, "BCTAB", "<from>,<to-voltage>[,<true=charging>,<precison>]", "Create a table for the given voltage range");
#endif
#if IOT_SENSOR_HAVE_BATTERY_RECORDER
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BREC, "BREC", "[<host=192.168.0.3>,<port=19523>,<type>]", "Enable sensor data recording");
#endif

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Sensor_Battery::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
    #if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        PROGMEM_AT_MODE_HELP_COMMAND(BCAP),
        PROGMEM_AT_MODE_HELP_COMMAND(BCTAB),
    #endif
    #if IOT_SENSOR_HAVE_BATTERY_RECORDER
        PROGMEM_AT_MODE_HELP_COMMAND(BREC),
    #endif
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool Sensor_Battery::atModeHandler(AtModeArgs &args)
{
#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCAP))) {
        auto voltage = args.toFloat(0);
        auto precision = args.toFloatMinMax(1, 1.0, 100.0, 2.0);
        auto start = micros();
        float capacity = Sensor_Battery::calcLipoCapacity(voltage, 1, false, precision);
        auto dur = micros() - start;
        start = micros();
        float charging = Sensor_Battery::calcLipoCapacity(voltage, 1, true, precision);
        auto dur2 = micros() - start;
        int digits = ceil(log(precision) / log(10));
        args.printf_P(PSTR("voltage=%.4fV capacity=%.*f%% (%uµs) charging=%.*f%% (%uµs) Umax=%.4f precision=%.2f"), voltage, digits, capacity, dur, digits, charging, dur2, Sensor_Battery::maxVoltage, precision);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCTAB))) {
        auto start = args.toFloat(0);
        auto end = args.toFloat(1);
        auto charging = args.isTrue(2);
        auto precision = args.toFloatMinMax(3, 1.0, 100.0, 2.0);
        int digits = ceil(log(precision) / log(10));
        if (start > end) {
            std::swap(start, end);
        }
        if (start == 0) {
            start = 3.5;
        }
        if (end == 0) {
            end =  4.25;
        }
        if (start < 2.5) {
            start = 2.5;
        }
        if (end > 4.35) {
            end = 4.35;
        }
        args.printf_P(PSTR("Generating table for %.2-%.2fV"), start, end);
        auto &stream = args.getStream();
        float prev = -1;
        stream.printf_P("voltage,level\n");
        for(float v = start; v <= end; v += 0.0001) {
            auto n = Sensor_Battery::calcLipoCapacity(v, 1, charging, precision);
            if (n != prev) {
                prev = n;
                stream.printf_P("%.*f,%.*f\n", (digits / 2) + 3, v, digits, n);
                delay(5);
            }
        }
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

