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

    // Fixed part of the conversion formulas below, folded once at compile time.
    // All three values are "constant / pulseWidthUs", so a reading is a single
    // multiplication and division instead of multiplying every literal on the fly.
    //   U = kScaleU / pulseUs,  I = kScaleI / pulseUs,  P = kScaleP / pulseUs
    // (fosc is in MHz and pulseUs in us, so the 1e6 of the datasheet formulas is implicit)
    static constexpr float kScaleU = (128.0 * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_V_RES_DIV) / IOT_SENSOR_HLW80xx_F_OSC;
    static constexpr float kScaleI = (32.0 * IOT_SENSOR_HLW80xx_VREF) / (3.0 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_SHUNT);
    static constexpr float kScaleP = (4.0 * IOT_SENSOR_HLW80xx_V_RES_DIV * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_VREF) / (3.0 * IOT_SENSOR_HLW80xx_SHUNT * IOT_SENSOR_HLW80xx_F_OSC);

    // Energy of a single counter increment (one CF edge) in kWh: the power for a 1e6 us
    // pulse width divided by the Ws of one kWh. Derived from kScaleP instead of
    // evaluating the power formula with pulse = 1e6 us for every reading.
    static constexpr float kEnergyPerCount = kScaleP / (1000000.0 * 1000.0 * 3600.0);
    // Inverse of kEnergyPerCount to convert a saved kWh value back into pulses
    static constexpr float kCountPerKwh = (1000000.0 * 1000.0 * 3600.0) / kScaleP;

}

// HLW8012 single-phase energy metering IC
// Datasheet: https://datasheet.lcsc.com/szlcsc/1811151452_Hiliwei-Tech-HLW8012_C83804.pdf
//
// HOW THE CHIP REPORTS DATA
//   The chip has no digital readout. It outputs pulse trains whose frequency is
//   proportional to what is being measured, so we time the pulses and convert
//   the frequency back into a real-world value.
//     CF  pin: frequency proportional to active power
//     CF1 pin: frequency proportional to current OR voltage, selected by the SEL pin
//              (SEL low = current, SEL high = voltage)
//
// PULSE WIDTH -> FREQUENCY
//   The pulses have a 50% duty cycle, so one measured pulse width is half a period:
//     period = 2 * pulseWidthUs / 1e6 seconds
//     f      = 1e6 / (2 * pulseWidthUs) Hz
//   (Older versions of this comment called the measured value "DutyCycleUs".
//   It is a pulse width in microseconds, not a duty cycle.)
//
// SYMBOLS
//   fosc     = 3579000 Hz  internal oscillator (3.579 MHz)
//   Vref     = 2.43 V      internal reference voltage
//   Rshunt   = shunt resistance in ohms
//   Rdivider = voltage divider ratio, mains voltage / V2,
//              i.e. (R_top + R_bottom) / R_bottom, not a single resistor value
//   V1       = current * Rshunt    voltage across the V1P/V1N pins
//   V2       = voltage / Rdivider  voltage at the V2P pin
//   The chip only sees these small scaled-down voltages. The shunt turns current
//   into a voltage, the divider shrinks mains voltage to a safe level, and the
//   final formulas below undo both.
//
// HOW THE FORMULAS BELOW WERE DERIVED
//   For each pin, take the datasheet's "frequency as a function of input voltage"
//   equation, set it equal to the measured frequency (1e6 / (2 * pulseUs)), and
//   solve for the real quantity. The odd constants are just the datasheet gains
//   combined with the 1e6 / 2 from the pulse-width conversion.
//   Example (power): 1e6 * 128 / (2 * 48) = 4e6 / 3
//
// POWER (CF pin)
//   Datasheet: f_CF = (48 * V1 * V2 / Vref^2) * (fosc / 128)
//   Solving for V1 * V2 and using power = voltage * current = V1 * V2 * Rdivider / Rshunt:
//     power = (4e6 * Rdivider * Vref^2) / (3 * cfPulseUs * Rshunt * fosc)
//   Note: this is power directly. Dividing it by current gives voltage, but that
//   is only an intermediate step, not a separate way to measure voltage.
//
// CURRENT (CF1 pin, current mode)
//   Datasheet: f_CF1 = (24 * V1 / Vref) * (fosc / 512)
//     current = (32e6 * Vref) / (3 * cf1PulseUs * Rshunt * fosc)
//
// VOLTAGE (CF1 pin, voltage mode)
//   Datasheet: f_CF1 = (2 * V2 / Vref) * (fosc / 512)
//     voltage = (128e6 * Vref * Rdivider) / (cf1PulseUs * fosc)
//
// SANITY CHECK
//   power from the CF formula should roughly equal voltage * current from the
//   two CF1 formulas, which is a handy way to verify Rshunt and Rdivider.

// pulseWidthUs is the pulse width in µs (50% PWM)
// The constant parts of the formulas are the pre-folded HLW80xx::kScale* values, so a reading
// costs one multiplication and one division (the calibration is the only runtime input):
//     U = (128 * Vref * Rdivider * _calibrationU) / (pulseWidthUs * fosc)           -> _calibrationU * kScaleU / pulseWidthUs
//     I = (32 * Vref * _calibrationI) / (pulseWidthUs * 3 * fosc * Rshunt)           -> _calibrationI * kScaleI / pulseWidthUs
//     P = (4 * Rdivider * Vref^2 * _calibrationP) / (pulseWidthUs * 3 * Rshunt * fosc) -> _calibrationP * kScaleP / pulseWidthUs
#define IOT_SENSOR_HLW80xx_CALC_U(pulseWidthUs) ((_calibrationU * HLW80xx::kScaleU) / (pulseWidthUs))
#define IOT_SENSOR_HLW80xx_CALC_I(pulseWidthUs) ((_calibrationI * HLW80xx::kScaleI) / (pulseWidthUs))
#define IOT_SENSOR_HLW80xx_CALC_P(pulseWidthUs) ((_calibrationP * HLW80xx::kScaleP) / (pulseWidthUs))

// count is incremented on falling and raising edge
#define IOT_SENSOR_HLW80xx_PULSE_TO_KWH(count) ((count) * (HLW80xx::kEnergyPerCount * _calibrationP))
#define IOT_SENSOR_HLW80xx_KWH_TO_PULSE(kwh)   ((kwh) * HLW80xx::kCountPerKwh / _calibrationP)

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
