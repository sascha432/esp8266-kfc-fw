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

#define USE_ADC_MANAGER 1

float Sensor_Battery::maxVoltage = 0;

using KFCConfigurationClasses::System;
using SensorRecordType = KFCConfigurationClasses::Plugins::SensorConfigNS::SensorRecordType;

uint8_t operator&(SensorRecordType a, SensorRecordType b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

uint8_t operator&(uint8_t a, SensorRecordType b) {
    return a & static_cast<uint8_t>(b);
}


Sensor_Battery::Sensor_Battery(const String &name) :
    MQTT::Sensor(MQTT::SensorType::BATTERY),
    _name(name),
    _adcValue(0)
{
    REGISTER_SENSOR_CLIENT(this);
    reconfigure(nullptr);

    #if USE_ADC_MANAGER
        ADCManager::getInstance().addAutoReadTimer(Event::milliseconds(kReadInterval / 2), Event::milliseconds(10), kReadInterval / 20);
    #endif

    _Timer(_timer).add(Event::milliseconds(kReadInterval), true, [this](Event::CallbackTimerPtr) {
        _readADC();
    });
}

Sensor_Battery::~Sensor_Battery()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

void Sensor_Battery::_readADC(uint8_t num)
{
    #if USE_ADC_MANAGER
        auto value = ADCManager::getInstance().readValue();
    #else
        uint32_t tmp = analogRead(A0);
        auto count = num;
        while(--count) {
            delay(1);
            tmp += analogRead(A0);
        }
        uint16_t value = tmp / num;
    #endif
    if (_adcValue == 0) {
        _adcValue = value;
    }
    else {
        _adcValue = ((_adcValue * 10.0) + value) / 11.0;
    }
    _status.updateSensor(*this);
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
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(static_cast<AutoDiscoveryNumHelperType>(num)) {
        case AutoDiscoveryNumHelperType::VOLTAGE:
            if (discovery->create(this, _getId(TopicType::VOLTAGE), format)) {
                discovery->addStateTopic(_getTopic(TopicType::VOLTAGE));
                discovery->addDeviceClass(F("voltage"), 'V');
                discovery->addName(_name);
                discovery->addObjectId(baseTopic + MQTT::Client::filterString(_name, true));
            }
            break;
        #if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
            case AutoDiscoveryNumHelperType::LEVEL:
                if (discovery->create(this, _getId(TopicType::LEVEL), format)) {
                    discovery->addStateTopic(_getTopic(TopicType::LEVEL));
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
    __LDBG_printf("client=%p connected=%u voltage=%.3f", client, client && client->isConnected() ? 1 : 0, _status.getVoltage());
    if (isConnected() && _status.getVoltage()) {
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

    group.end();
}

void Sensor_Battery::reconfigure(PGM_P source)
{
    _config = Plugins::Sensor::getConfig().battery;
    maxVoltage = 0;
}

void Sensor_Battery::Status::updateSensor(Sensor_Battery &sensor)
{
    float voltage = ((IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * sensor._adcValue) + (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R2 * sensor._adcValue)) / (IOT_SENSOR_BATTERY_VOLTAGE_DIVIDER_R1 * ADCManager::kMaxADCValue);
    maxVoltage = std::max(maxVoltage, voltage);
    // apply ADC calibration
    _voltage = voltage * sensor._config.calibration + sensor._config.offset;

    #ifdef IOT_SENSOR_BATTERY_CHARGING
        #if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN != -1
            if (IOT_SENSOR_BATTERY_CHARGING) {
                #if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN == 3
                    // Serial RX is used as input, this might malfunction if a serial2usb converter is attached
                    ETS_UART_INTR_DISABLE();
                    pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT_PULLUP);
                #endif
                auto value = digitalRead(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN);
                #if IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN == 3
                    pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT);
                    ETS_UART_INTR_ENABLE();
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
        _level = IOT_SENSOR_BATTERY_LEVEL_FUNCTION(_voltage,  IOT_SENSOR_BATTERY_NUM_CELLS, _charging == ChargingType::CHARGING);
    #endif

    #if IOT_SENSOR_BATTERY_DISPLAY_POWER_STATUS
        _state = IOT_SENSOR_BATTERY_ON_EXTERNAL ? StateType::RUNNING_ON_EXTERNAL_POWER : StateType::RUNNING;
    #endif
}

float Sensor_Battery::calcLipoCapacity(float voltage, uint8_t cells, bool charging)
{
    static constexpr auto coefficients PROGMEM = stdex::array_of<const float>(
        -163.25,
        915.25,
        -1851.1,
        1741.2,
        -745.8,
        119.98
    );
    voltage /= cells;
    if (charging) {
        voltage -= (4.25 - voltage) * 0.15;
    }
    float powValue = 1;
    float percent = 0;
    for(auto coefficient: coefficients) {
        percent += coefficient * powValue;
        powValue *= voltage;
    }
    return std::clamp<float>(percent / 247.625, 0, 100);
}

#if AT_MODE_SUPPORTED && IOT_SENSOR_BATTERY_DISPLAY_LEVEL

#include "at_mode.h"

#if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCAP, "BCAP", "<voltage>", "Calculate battery capacity for given voltage");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BCTAB, "BCTAB", "<from>,<to-voltage>[,<true=charging>]", "Create a table for the given voltage range");
#endif

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Sensor_Battery::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
    #if IOT_SENSOR_BATTERY_DISPLAY_LEVEL
        PROGMEM_AT_MODE_HELP_COMMAND(BCAP),
        PROGMEM_AT_MODE_HELP_COMMAND(BCTAB),
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
            auto start = micros();
            float capacity = Sensor_Battery::calcLipoCapacity(voltage, 1, false);
            auto dur = micros() - start;
            start = micros();
            float charging = Sensor_Battery::calcLipoCapacity(voltage, 1, true);
            auto dur2 = micros() - start;
            args.printf_P(PSTR("voltage=%.4fV capacity=%.2f%% (%uµs) charging=%.2f%% (%uµs) Umax=%.4f"), voltage, capacity, dur, charging, dur2, Sensor_Battery::maxVoltage);
            return true;
        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BCTAB))) {
            auto start = args.toFloat(0);
            auto end = args.toFloat(1);
            auto charging = args.isTrue(2);
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
            for(float v = start; v <= end; v += 0.001) {
                auto n = Sensor_Battery::calcLipoCapacity(v, 1, charging);
                if (n != prev) {
                    prev = n;
                    stream.printf_P("%.3f,%.2f\n", v, n);
                    delay(5);
                }
            }
            return true;
        }
    #endif
        return false;
}

#endif

#endif

