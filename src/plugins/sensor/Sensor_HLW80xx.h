/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032

// support for
// HLW8012 (and SSP1837, CSE7759, ...)
// HLW8032 (and CSE7759B, ...) ***not fully implemented***

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include "plugins/http2serial/http2serial.h"

// Simple calibration with 2 multimeters:
//
// - One multimeter measures the voltage at the input, the other one the current that goes into the dimmer
// - Attach a 60W incandescent bulb to the dimmer
// - Turn the brightness to 0% and type +SP_HLWCAL=U,5 into the console and watch the voltage on the multimeter
//   for a few seconds. Both readings should be pretty stable, if not repeat the step
// - Enter the voltage from the console and the multimeter readings with +SP_HLWCAL=U,106.353607,119.27
// - Turn brightness to 100% and let the bulb warm up until the current readings are stable
// - Type +SP_HLWCAL=I,10 and watch the current on the multimeter
// - Enter the current from the console and the multimeter with +SP_HLWCAL=I,0.451026,0.493
// - Type +SP_HLWCAL=P,5 into the console and read both current and voltage from the multimeters
// - Enter the power from the console, the voltage and current readings from the multimeters with +SP_HLWCAL=P,49.521816,119.0,0.493
// - Check if the dimmer shows the correct readings and store the calibration with +STORE
//
// While turned off the current should be ~0.009A with a PF of 0.54. 0.0093A * 117.2V * 0.54 = ~0.589W
// This requires a shunt <=0.005R, otherwise the current will be higher and the PF incorrect
// For the HLK-PM03 3W version I had quite a big difference ranging from 0.52-0.61W
//
// 	        measurement range
// shunt      mA        A     absolute max. rating (A)
// 0.001    40.0    43.00     2000
// 0.002    20.0    21.50     1000
// 0.003    13.3    14.33     667
// 0.005     8.0     8.60     400
// 0.008     5.0     5.38     250
// 0.01      4.0     4.30     200
// 0.05      0.8     0.86      40

// enables output for the HLW8012 live graph
#ifndef IOT_SENSOR_HLW80xx_DATA_PLOT
#    define IOT_SENSOR_HLW80xx_DATA_PLOT 0
#endif

// voltage divider for V2P
#ifndef IOT_SENSOR_HLW80xx_V_RES_DIV
#    define IOT_SENSOR_HLW80xx_V_RES_DIV ((4 * 470) / 1.0) // 4x470K : 1K
#endif

// current shunt resistance
#ifndef IOT_SENSOR_HLW80xx_SHUNT
#    define IOT_SENSOR_HLW80xx_SHUNT 0.008
#endif

// compensate current when the load is dimmed or switched off
#ifndef IOT_SENSOR_HLW80xx_ADJUST_CURRENT
#    define IOT_SENSOR_HLW80xx_ADJUST_CURRENT 0
#endif

// adjust non linear error of the sensor
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
#    define IOT_SENSOR_HLW80xx_ADJ_I_CALC(level, current) ( \
        level < 0 ? current : (level == 0 ? ((IOT_SENSOR_HLW80xx_MIN_CURRENT > 0.009) ? 0.009f : current) : (level >= 1 ? current : (current * (1.0 / (1.013292 + (0.7352072 - 1.013292) / (1 + pow(level / 0.04869815, 1.033051))))))))
#else
#    define IOT_SENSOR_HLW80xx_ADJ_I_CALC(level, current) current
#endif

// this option can be used to add a noise detection algorithm. it is only required if
// no load is connected to the shunt. this scenario should be prevented by adding a minimum
// load after the shunt. for example the HLW8012 power supply or/and a 470K-2M load resistor
// depending on the shunt value
#ifndef IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
#    define IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION 0
#endif

// 40µV input offset voltage
#ifndef IOT_SENSOR_HLW80xx_INPUT_OFS_U
#    define IOT_SENSOR_HLW80xx_INPUT_OFS_U 0.00004
#endif

// +-43.5mV differential input voltage (max. ratings +-2.0V)
#ifndef IOT_SENSOR_HLW80xx_DIFF_INPUT_U
#    define IOT_SENSOR_HLW80xx_DIFF_INPUT_U 0.0435
#endif

// maximum noise level, 1000 = 1.0
#ifndef IOT_SENSOR_HLW80xx_MAX_NOISE
#    define IOT_SENSOR_HLW80xx_MAX_NOISE 40000
#endif

#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
#    define IOT_SENSOR_HLW80xx_NO_NOISE(level) (level < IOT_SENSOR_HLW80xx_MAX_NOISE)
#else
#    define IOT_SENSOR_HLW80xx_NO_NOISE(level) true
#endif

#define IOT_SENSOR_HLW80xx_MIN_CURRENT (IOT_SENSOR_HLW80xx_INPUT_OFS_U / IOT_SENSOR_HLW80xx_SHUNT)
#define IOT_SENSOR_HLW80xx_MAX_CURRENT (IOT_SENSOR_HLW80xx_DIFF_INPUT_U / IOT_SENSOR_HLW80xx_SHUNT)

#define IOT_SENSOR_HLW80xx_CURRENT_MIN_PULSE (uint32_t)((32.0 * IOT_SENSOR_HLW80xx_VREF) / (3.0 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_MAX_CURRENT * IOT_SENSOR_HLW80xx_SHUNT))
#define IOT_SENSOR_HLW80xx_CURRENT_MAX_PULSE (uint32_t)((32.0 * IOT_SENSOR_HLW80xx_VREF) / (3.0 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_MIN_CURRENT * IOT_SENSOR_HLW80xx_SHUNT))

// update rate WebUI
#ifndef IOT_SENSOR_HLW80xx_UPDATE_RATE
#    define IOT_SENSOR_HLW80xx_UPDATE_RATE 2
#endif

// update rate MQTT
#ifndef IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT
#    define IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT 60
#endif

// internal voltage reference
#ifndef IOT_SENSOR_HLW80xx_VREF
#    define IOT_SENSOR_HLW80xx_VREF 2.43
#endif

// oscillator frequency in MHz
#ifndef IOT_SENSOR_HLW80xx_F_OSC
#    define IOT_SENSOR_HLW80xx_F_OSC 3.579000
#endif

// interval in milliseconds to save energy counter, 0 to disable
#ifndef IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
#    define IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT (5 * 60 * 1000)
#endif

#define IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_FS      1 // save to file system
#define IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_NVS     2 // save to config nvs, fallback is the file system

// select storage type
#ifndef IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE
#   define IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_NVS
#endif

// save backup to file system every IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT writes (hourly unless the data does not change)
#ifndef IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
#    define IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS ((60 * 60 * 1000) / IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT)
#endif

#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE != IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_NVS
#    undef IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
#    define IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS 0
#endif

#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT != 0 && IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT < 60000
#    error IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT must be greater or equal 60000
#endif

#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS != 0 && IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS < 1
#    error IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS must be greater than 0
#endif

// number of energy counters, must be >= 2
#ifndef IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS
#    define IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS 2
#endif

namespace HLW80xx {

    static constexpr uint32_t kSaveEnergyIntervalMillis = IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;
    static constexpr uint32_t kSaveEnergyFSCounter = IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS;

    static constexpr float kMinCurrentAmps = IOT_SENSOR_HLW80xx_MIN_CURRENT;
    static constexpr float kMaxCurrentAmps = IOT_SENSOR_HLW80xx_MAX_CURRENT;

    static constexpr uint32_t kCurrentMinPulse = IOT_SENSOR_HLW80xx_CURRENT_MIN_PULSE;
    static constexpr uint32_t kCurrentMaxPulse = IOT_SENSOR_HLW80xx_CURRENT_MAX_PULSE;

}

// https://datasheet.lcsc.com/szlcsc/1811151452_Hiliwei-Tech-HLW8012_C83804.pdf

// Fcf = (((v1 * v2) * 48) / (Vref * Vref)) * (fosc / 128), v1 = (current * Rshunt), v2 = (voltage / Rdivider), power = voltage * current, Fcf = 1 / (FcfDutyCycleUs * 2 / 1000000)
//  (current * Rshunt) * (voltage / Rdivider) = (4000000 * Vref * Vref) / (FcfDutyCycleUs * 3 * fosc)
//  voltage = (4000000 * Rdivider * Vref * Vref) / (3 * FcfDutyCycleUs * Rshunt * fosc * current), voltage = power / current
//  power = (4000000 * Rdivider * Vref * Vref) / (3 * FcfDutyCycleUs * Rshunt * fosc)

// Fcfi = ((v1 * 24) / Vref) * (fosc / 512), v1 = current * Rshunt
//  1 / (FcfiDutyCycleUs * 2 / 1000000) = (((current * Rshunt) * 24) / Vref) * (fosc / 512)
//  current = (32000000 * Vref) / (FcfiDutyCycleUs * 3 * Rshunt * fosc)

// Fcfu = ((v2 * 2) / Vref) * (fosc / 512), v2 = voltage / Rdivider
//  1 / (FcfuDutyCycleUs * 2 / 1000000) = ((voltage / Rdivider * 2) / Vref) * (fosc / 512)
//  voltage = (128000000 * Vref * Rdivider) / (FcfuDutyCycleUs * fosc)

// v1 = voltage @ v1p/v1n
// v2 = voltage @ v2p
// fosc = 3.579Mhz
// Vref = 2.43V

// pulse is the duty cycle in µs (50% PWM)
#define IOT_SENSOR_HLW80xx_CALC_U(pulse) (((128.0 * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_V_RES_DIV) * _calibrationU) / (pulse * IOT_SENSOR_HLW80xx_F_OSC))
#define IOT_SENSOR_HLW80xx_CALC_I(pulse) (((32.0 * IOT_SENSOR_HLW80xx_VREF) * _calibrationI) / (pulse * (3.0 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_SHUNT)))
#define IOT_SENSOR_HLW80xx_CALC_P(pulse) (((4.0 * IOT_SENSOR_HLW80xx_V_RES_DIV * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_VREF) * _calibrationP) / (pulse * (3.0 * IOT_SENSOR_HLW80xx_SHUNT * IOT_SENSOR_HLW80xx_F_OSC)))

// count is incremented on falling and raising edge
#define IOT_SENSOR_HLW80xx_PULSE_TO_KWH(count) (count * IOT_SENSOR_HLW80xx_CALC_P(1000000.0) / (1000.0 * 3600.0))
#define IOT_SENSOR_HLW80xx_KWH_TO_PULSE(kwh)   (((1000.0 * 3600.0) * kwh) / IOT_SENSOR_HLW80xx_CALC_P(1000000.0))

class Sensor_HLW8012;
class Sensor_HLW8032;

class Sensor_HLW80xx : public MQTT::Sensor {
public:
    using EnergyCounterArray = std::array<uint64_t, IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS>;
    using ConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::HLW80xxConfigType;

    #if IOT_SENSOR_HAVE_HLW8012
        using SensorSubType = Sensor_HLW8012;
        static constexpr auto kSensorType = MQTT::SensorType::HLW8012;
    #elif IOT_SENSOR_HAVE_HLW8032
        using SensorSubType = Sensor_HLW8032;
        static constexpr auto kSensorType = MQTT::SensorType::HLW8032;
    #endif

public:
    Sensor_HLW80xx(const String &name, MQTT::SensorType type);

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;

    virtual String _getId(const __FlashStringHelper *type = nullptr) {
        return String();
    }

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form);
    virtual void configurationSaved(FormUI::Form::BaseForm *form);

    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

#if AT_MODE_SUPPORTED
    #if AT_MODE_HELP_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
    #endif
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

    EnergyCounterArray &getEnergyCounters();
    uint64_t &getEnergyPrimaryCounter();
    uint64_t &getEnergySecondaryCounter();

    virtual void dump(Print &output);

protected:
    // store energy counters in file
    void __saveEnergyCounterToFile();
    // load energy counters from FS into 'energy' and report result
    bool __loadEnergyCounterFromFile(EnergyCounterArray &energy);
    // load energy counters into 'energy' and report result
    bool __loadEnergyCounter(EnergyCounterArray &energy);

    // store energy counters
    // use shutdown = true to ignore any timeouts and store even if contents are the same
    void _saveEnergyCounter(bool shutdown = false);
    // load energy counter
    void __loadEnergyCounter();
    // schedule to store energy counters
    void _resetSaveEnergyTimer();
    // increase all energy counters
    void _incrEnergyCounters(uint32_t count);

    WebUINS::TrimmedFloat _currentToNumber(float current) const;
    WebUINS::TrimmedFloat _energyToNumber(float energy) const;
    WebUINS::TrimmedFloat _powerToNumber(float power) const;

    float _getPowerFactor() const;
    float _getEnergy(uint8_t num = 0) const;

    String _getTopic();

protected:
    String _name;
    float _power;
    float _voltage;
    float _current;

    float _calibrationI;
    float _calibrationU;
    float _calibrationP;
    uint8_t _extraDigits;

    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
            uint16_t _saveEnergyCounterFSCounter;
        #endif
        uint32_t _saveEnergyCounterTimer;
    #endif

public:
    Event::Timer _dumpTimer;

    static void setExtraDigits(uint8_t digits);
    EnergyCounterArray _energyCounter;

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
protected:
    float _dimmingLevel;

public:
    // can be used to compensate the current when the load is dimmed
    // -1 to disable
    void setDimmingLevel(float dimmingLevel);
#endif

#if IOT_SENSOR_HLW80xx_DATA_PLOT
protected:
    typedef enum {
        VOLTAGE = 0x0001,
        CURRENT = 0x0002,
        POWER = 0x0004,
        CONVERT_UNIT = 0x8000,
    } WebSocketDataTypeEnum_t;

    WebSocketDataTypeEnum_t _getWebSocketPlotData() const {
        return _webSocketPlotData;
    }
    std::vector<float> _plotData;
    uint32_t _plotDataTime;

private:
    AsyncWebSocketClient *_webSocketClient;
    WebSocketDataTypeEnum_t _webSocketPlotData;
#endif
};

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT

inline void Sensor_HLW80xx::setDimmingLevel(float dimmingLevel)
{
    _dimmingLevel = dimmingLevel;
}

#endif

inline Sensor_HLW80xx::EnergyCounterArray &Sensor_HLW80xx::getEnergyCounters()
{
    return _energyCounter;
}

inline uint64_t &Sensor_HLW80xx::getEnergyPrimaryCounter()
{
    return _energyCounter[0];
}

inline uint64_t &Sensor_HLW80xx::getEnergySecondaryCounter()
{
    return _energyCounter[1];
}

inline void Sensor_HLW80xx::_incrEnergyCounters(uint32_t count)
{
    for(auto &value: _energyCounter) {
        value += count;
    }
}

inline WebUINS::TrimmedFloat Sensor_HLW80xx::_currentToNumber(float current) const
{
    uint8_t digits = 2;
    if (current < 1) {
        digits = 3;
    }
    return WebUINS::TrimmedFloat(current, digits + _extraDigits);
}

inline WebUINS::TrimmedFloat Sensor_HLW80xx::_energyToNumber(float energy) const
{
    char buf[8];
    auto digits = energy < 1 ? 4 : std::max(0, 4 - snprintf_P(buf, sizeof(buf), PSTR("%u"), static_cast<uint32_t>(energy)));
    return WebUINS::TrimmedFloat(energy, digits + _extraDigits);
}

inline WebUINS::TrimmedFloat Sensor_HLW80xx::_powerToNumber(float power) const
{
    return WebUINS::TrimmedFloat(power, ((power < 10) ? 2 : 1) + _extraDigits);
}

inline float Sensor_HLW80xx::_getPowerFactor() const
{
    return (isnan(_power) || isnan(_voltage) || isnan(_current) || _current == 0) ? 0 : std::min(_power / (_voltage * _current), 1.0f);
}

inline float Sensor_HLW80xx::_getEnergy(uint8_t num) const
{
    return (num >= IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS) ? NAN : IOT_SENSOR_HLW80xx_PULSE_TO_KWH(_energyCounter[num]);
}

inline String Sensor_HLW80xx::_getTopic()
{
    return MQTT::Client::formatTopic(_getId());
}


#endif
