/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR && (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)

// support for
// HLW8012 (and SSP1837, CSE7759, ...)
// HLW8032 (and CSE7759B, ...)

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include "plugins/http2serial/http2serial.h"

// Simple calibration with 2 multimeters:
//
// - Set all 3 calibration values to 1.0, extra digits to 2
// - Attach a 60W incandescent bulb to the dimmer
// - One multimeter measures the voltage at the input, the other one the current that goes into the dimmer
// - Turn the dimmer off and measure the voltage. Divide the voltage the dimmer displays by the mesured voltage and enter the value
//   as voltage calibration. i.e. 117.2V / 106.11V = 1.10452. After saving it should display the correct voltage
// - Set brightness to 100%, wait until the bulb has warmed up and the current is stable, then read both multimeters. 117.2V * 0.492A = 57.6624W
// - Divide the current the dimmer displays by the measured current and enter the value as current calibration. 0.465A / 0.492A = 0.9451
// - After saving the dimmer should display the correct current
// - Finally divide the calculated power by the power the dimmer displays and save the value as power calibration. 57.6624W / 53.4W = 1.07982
//
// While turned off the current should be ~0.009A with a PF of 0.54. 0.0093A * 117.2V * 0.54 = ~0.589W
// This requires a shunt >=0.005R, otherwise the current will be higher and the PF incorrect
//
// Min/max current that can be measured with different shunts
// shunt    min.    max.
// 0.001	0.040	43.0
// 0.002	0.020	21.5
// 0.003	0.013	14.3
// 0.005	0.008	8.6
// 0.008	0.005	5.4
// 0.01	    0.004	4.3
// 0.02	    0.002	2.15
// 0.05	    0.001	0.86


// enables output for the HLW8012 live graph
#ifndef IOT_SENSOR_HLW80xx_DATA_PLOT
#define IOT_SENSOR_HLW80xx_DATA_PLOT                    1
#endif

// voltage divider for V2P
#ifndef IOT_SENSOR_HLW80xx_V_RES_DIV
#define IOT_SENSOR_HLW80xx_V_RES_DIV                    ((4 * 470) / 1.0)               // 4x470K : 1K
#endif

// current shunt resistance
#ifndef IOT_SENSOR_HLW80xx_SHUNT
#define IOT_SENSOR_HLW80xx_SHUNT                        0.001
#endif

// this option can be used to add a noise detection algorithm. it is only required if
// no load is connected to the shunt. this scenario should be prevented by adding a minimum
// load after the shunt. for example the HLW8012 power supply or/and a 470K-2M load resistor
// depending on the shunt value
#ifndef IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
#define IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION            0
#endif

// 40µV input voltage offset
#ifndef IOT_SENSOR_HLW80xx_SHUNT_NOISE_U
#define IOT_SENSOR_HLW80xx_SHUNT_NOISE_U                0.00004
#endif

// ~180ms pulse length. my tested sensors go down to 210-230ms before heavy noise kicks in
#define IOT_SENSOR_HLW80xx_SHUNT_NOISE_PULSE            (uint32_t)((32.0 * IOT_SENSOR_HLW80xx_VREF) / (3 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_SHUNT_NOISE_U))
#define IOT_SENSOR_HLW80xx_MAX_NOISE                    40000

#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
#define IOT_SENSOR_HLW80xx_NO_NOISE(level)              (level < IOT_SENSOR_HLW80xx_MAX_NOISE)
#else
#define IOT_SENSOR_HLW80xx_NO_NOISE(level)              true
#endif

#define IOT_SENSOR_HLW80xx_MIN_CURRENT                  (IOT_SENSOR_HLW80xx_SHUNT_NOISE_U / IOT_SENSOR_HLW80xx_SHUNT)
#define IOT_SENSOR_HLW80xx_MAX_CURRENT                  (0.043 / IOT_SENSOR_HLW80xx_SHUNT)

// update rate WebUI
#ifndef IOT_SENSOR_HLW80xx_UPDATE_RATE
#define IOT_SENSOR_HLW80xx_UPDATE_RATE                  2
#endif

// update rate MQTT
#ifndef IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT
#define IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT             30
#endif

// internal voltage reference
#ifndef IOT_SENSOR_HLW80xx_VREF
#define IOT_SENSOR_HLW80xx_VREF                         2.43
#endif

// oscillator frequency in MHz
#ifndef IOT_SENSOR_HLW80xx_F_OSC
#define IOT_SENSOR_HLW80xx_F_OSC                        3.579000
#endif

// interval to save energy counter to SPIFFS, 0 to disable
#ifndef IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
#define IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT              (5 * 60 * 1000)
#endif

// number of energy counters, must be >= 2
#ifndef IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS
#define IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS          5
#endif

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
#define IOT_SENSOR_HLW80xx_CALC_U(pulse)                (((128.0 * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_V_RES_DIV) * _calibrationU) / (pulse * IOT_SENSOR_HLW80xx_F_OSC))
#define IOT_SENSOR_HLW80xx_CALC_I(pulse)                ((32.0 * IOT_SENSOR_HLW80xx_VREF) / (pulse * ((3.0 * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_SHUNT) * _calibrationI)))
#define IOT_SENSOR_HLW80xx_CALC_PULSE_I(current)        (32.0 * IOT_SENSOR_HLW80xx_VREF) / ((3.0 * current * IOT_SENSOR_HLW80xx_F_OSC * IOT_SENSOR_HLW80xx_SHUNT) * _calibrationI)
#define IOT_SENSOR_HLW80xx_CALC_P(pulse)                (((4.0 * IOT_SENSOR_HLW80xx_V_RES_DIV * IOT_SENSOR_HLW80xx_VREF * IOT_SENSOR_HLW80xx_VREF) * _calibrationP) / (pulse * _calibrationI * (3.0 * IOT_SENSOR_HLW80xx_SHUNT * IOT_SENSOR_HLW80xx_F_OSC)))

// count is incremented on falling and raising edge
#define IOT_SENSOR_HLW80xx_PULSE_TO_KWH(count)          (count * IOT_SENSOR_HLW80xx_CALC_P(1000000.0) / (1000.0 * 3600.0))
#define IOT_SENSOR_HLW80xx_KWH_TO_PULSE(kwh)            (((1000.0 * 3600.0) * kwh) / IOT_SENSOR_HLW80xx_CALC_P(1000000.0))

class Sensor_HLW8012;
class Sensor_HLW8032;

class Sensor_HLW80xx : public MQTTSensor {
public:
    Sensor_HLW80xx(const String &name);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;

    virtual String _getId(const __FlashStringHelper *type = nullptr) {
        return String();
    }

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form);

    virtual void reconfigure() override;
    virtual void restart() override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, AtModeArgs &args) override;
#endif

    void resetEnergyCounter();
    uint64_t *getEnergyCounters() {
        return _energyCounter;
    }
    uint64_t &getEnergyPrimaryCounter() {
        return _energyCounter[0];
    }
    uint64_t &getEnergySecondaryCounter() {
        return _energyCounter[1];
    }

    virtual void dump(Print &output);

protected:
    void _saveEnergyCounter();
    void _loadEnergyCounter();
    void _incrEnergyCounters(uint32_t count);

    JsonNumber _currentToNumber(float current) const;
    JsonNumber _energyToNumber(float energy) const;
    JsonNumber _powerToNumber(float power) const;

    float _getPowerFactor() const;
    float _getEnergy(uint8_t num = 0) const;

    String _getTopic();

    static bool _compareFuncHLW8012(MQTTSensor &sensor, Sensor_HLW8012 &) {
        return sensor.getType() == MQTTSensor::HLW8012;
    }
    static bool _compareFuncHLW8032(MQTTSensor &sensor, Sensor_HLW8032 &) {
        return sensor.getType() == MQTTSensor::HLW8032;
    }
    static bool _compareFunc(MQTTSensor &sensor, Sensor_HLW80xx &) {
        return (sensor.getType() == MQTTSensor::HLW8012 || sensor.getType() == MQTTSensor::HLW8032);
    }

    String _name;
    float _power;
    float _voltage;
    float _current;

    float _calibrationI;
    float _calibrationU;
    float _calibrationP;
    uint8_t _extraDigits;

    unsigned long _saveEnergyCounterTimeout;
    time_t _nextMQTTUpdate;

public:
    void setExtraDigits(int8_t digits) {
        _extraDigits = std::max((int8_t)0, std::min((int8_t)6, digits));
        config._H_W_GET(Config().sensor).hlw80xx.extraDigits = _extraDigits;
    }
    uint64_t _energyCounter[IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS];

#if IOT_SENSOR_HLW80xx_DATA_PLOT
protected:
    typedef enum {
        VOLTAGE = 0x0001,
        CURRENT = 0x0002,
        POWER = 0x0004,
        CONVERT_UNIT = 0x8000,
    } WebSocketDataTypeEnum_t;

    AsyncWebSocketClient *_getWebSocketClient() const;
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

#endif
