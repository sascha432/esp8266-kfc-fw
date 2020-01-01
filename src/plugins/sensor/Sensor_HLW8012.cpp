/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_HLW8012

#include <LoopFunctions.h>
#include "Sensor_HLW8012.h"
#include "sensor.h"
#include "MicrosTimer.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// Low level interrupt handling
// ------------------------------------------------------------------------

typedef enum {
    CBF_CF =                0x01,
    CBF_CF1 =               0x02,
    CBF_CF_SKIPPED =        0x04,
    CBF_CF1_SKIPPED =       0x08,
} CallbackFlagsEnum_t;

static Sensor_HLW8012 *sensor = nullptr;
static volatile unsigned long callbackTimeCF;
static volatile unsigned long callbackTimeCF1;
static volatile uint32_t energyCounter = 0;
static volatile uint8_t callbackFlag = 0;

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF()
{
    callbackTimeCF = micros();
    if (callbackFlag & CBF_CF) {
        callbackFlag |= CBF_CF_SKIPPED;
    }
    callbackFlag |= CBF_CF;
    energyCounter++;
}

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF1()
{
    callbackTimeCF1 = micros();
    if (callbackFlag & CBF_CF1) {
        callbackFlag |= CBF_CF1_SKIPPED;
    }
    callbackFlag |= CBF_CF1;
}

void Sensor_HLW8012::loop()
{
    if (sensor) {
        sensor->_loop();
    }
}

void Sensor_HLW8012::_loop()
{
    noInterrupts();
    if (callbackFlag & CBF_CF_SKIPPED) {
        callbackFlag &= ~(CBF_CF_SKIPPED|CBF_CF);
        if (_inputCF.lastPulse) {
            _inputCF.lastPulse = callbackTimeCF;
        }
        interrupts();
    }
    else if (callbackFlag & CBF_CF) {
        callbackFlag &= ~CBF_CF;
        auto tempCounter = energyCounter;
        energyCounter = 0;
        auto tmp = callbackTimeCF;
        interrupts();
        _callbackCF(tmp);
        _incrEnergyCounters(tempCounter);
    }
    else {
        interrupts();
    }

    noInterrupts();
    if (callbackFlag & CBF_CF1_SKIPPED) {
        callbackFlag &= ~(CBF_CF1_SKIPPED|CBF_CF1);
        if (_inputCF1.lastPulse) {
            _inputCF1.lastPulse = callbackTimeCF1;
        }
        interrupts();
    }
    else if (callbackFlag & CBF_CF1) {
        callbackFlag &= ~CBF_CF1;
        auto tmp = callbackTimeCF1;
        interrupts();
        _callbackCF1(tmp);
    }
    else {
        interrupts();
    }

    if (millis() > _inputCF.timeout) {
        _inputCF = SensorInput_t();
        _power = 0;
    }

    if (millis() > _inputCF1.timeout) {
        _inputCF1 = SensorInput_t();
        if (_output == CURRENT) {
            _current = 0;
        }
        else if (_output == VOLTAGE) {
            _voltage = NAN;
        }
    }
    if (millis() > _saveEnergyCounterTimeout) {
        _saveEnergyCounter();
    }
}

// ------------------------------------------------------------------------
// Sensor class
// ------------------------------------------------------------------------

Sensor_HLW8012::Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1) : Sensor_HLW80xx(name), _pinSel(pinSel), _pinCF(pinCF), _pinCF1(pinCF1)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW8012(): component=%p\n"), this);
#endif
    registerClient(this);
    _output = CURRENT;

    digitalWrite(_pinSel, _output);
    pinMode(_pinSel, OUTPUT);
    pinMode(_pinCF, INPUT);
    pinMode(_pinCF1, INPUT);

    if (sensor) {
        __debugbreak_and_panic_printf_P(PSTR("Only one instance of Sensor_HLW8012 supported\n"));
    }

    sensor = this;
    LoopFunctions::add(Sensor_HLW8012::loop);
    attachInterrupt(digitalPinToInterrupt(_pinCF), Sensor_HLW8012_callbackCF, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_pinCF1), Sensor_HLW8012_callbackCF1, CHANGE);
}

Sensor_HLW8012::~Sensor_HLW8012()
{
    LoopFunctions::remove(Sensor_HLW8012::loop);
    sensor = nullptr;
    detachInterrupt(digitalPinToInterrupt(_pinCF));
    detachInterrupt(digitalPinToInterrupt(_pinCF1));
}


void Sensor_HLW8012::getStatus(PrintHtmlEntitiesString &output)
{
    output.printf_P(PSTR("Power Monitor HLW8012, "));
    output.printf_P(PSTR("calibration U=%f, I=%f, P=%f"), _calibrationU, _calibrationI, _calibrationP);
}

Sensor_HLW8012::SensorEnumType_t Sensor_HLW8012::getType() const
{
    return HLW8012;
}

String Sensor_HLW8012::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("hlw8012_0x%04x"), (_pinSel << 10) | (_pinCF << 5) | (_pinCF1));
    if (type) {
        id.write('_');
        id.print(FPSTR(type));
    }
    return id;
}

void Sensor_HLW8012::_callbackCF(unsigned long micros)
{
    _calcPulseWidth(_inputCF, micros, IOT_SENSOR_HLW8012_TIMEOUT_P);
    if (_inputCF.pulseWidth && _inputCF.counter >= 3) {
        _power = IOT_SENSOR_HLW80xx_CALC_P(_inputCF.pulseWidthIntegral);
    }
    else {
        _power = 0;
    }
    // _debug_printf_P(PSTR("pulse width %f count %u power %f energy %u\n"), _inputCF.pulseWidthIntegral, _inputCF.counter, IOT_SENSOR_HLW80xx_CALC_P(_inputCF.pulseWidthIntegral), _energyCounter);
}

void Sensor_HLW8012::_callbackCF1(unsigned long micros)
{
    if (millis() < _inputCF1.delayStart) { // skip anything before to let sensor settle
        return;
    }
    _calcPulseWidth(_inputCF1, micros, _output == VOLTAGE ? IOT_SENSOR_HLW8012_TIMEOUT_U : IOT_SENSOR_HLW8012_TIMEOUT_I);
    if (_inputCF1.pulseWidth) {
        // update values if enough pulses have been counted
        if (_output == CURRENT && _inputCF1.counter >= 3) {
            _current = IOT_SENSOR_HLW80xx_CALC_I(_inputCF1.pulseWidthIntegral);
        }
        else if (_output == VOLTAGE && _inputCF1.counter >= 10) {
            _voltage = IOT_SENSOR_HLW80xx_CALC_U(_inputCF1.pulseWidthIntegral);
        }
    }
    if (millis() > _inputCF1.toggleTimer) { // toggle coltage/current measurement

        if (_output == CURRENT) {
            _debug_printf_P(PSTR("pulse width %f count %u current %f / %f energy %u\n"), _inputCF1.pulseWidthIntegral, _inputCF1.counter, IOT_SENSOR_HLW80xx_CALC_I(_inputCF1.pulseWidthIntegral), _current, (uint32_t)_energyCounter[0]);
        }
        else {
            _debug_printf_P(PSTR("pulse width %f count %u voltage %f / %f energy %u\n"), _inputCF1.pulseWidthIntegral, _inputCF1.counter, IOT_SENSOR_HLW80xx_CALC_U(_inputCF1.pulseWidthIntegral), _voltage, (uint32_t)_energyCounter[0]);
        }

        _inputCF1 = SensorInput_t();
        _inputCF1.delayStart = millis() + IOT_SENSOR_HLW8012_DELAY_START;
        if (_output == CURRENT) {
            _inputCF1.timeout = _inputCF1.delayStart + IOT_SENSOR_HLW8012_TIMEOUT_U;
            _inputCF1.toggleTimer = _inputCF1.delayStart + IOT_SENSOR_HLW8012_MEASURE_LEN_U;
            _output = VOLTAGE;
        } else {
            _inputCF1.timeout = _inputCF1.delayStart + IOT_SENSOR_HLW8012_TIMEOUT_I;
            _inputCF1.toggleTimer = _inputCF1.delayStart + IOT_SENSOR_HLW8012_MEASURE_LEN_I;
            _output = CURRENT;
        }
        digitalWrite(_pinSel, _output ? HIGH : LOW);
    }
}

void Sensor_HLW8012::_calcPulseWidth(SensorInput_t &input, unsigned long micros, uint16_t timeout)
{
    if (input.lastPulse && millis() < input.timeout) {
        input.pulseWidth = get_time_diff(input.lastPulse, micros);
        if (input.pulseWidthIntegral) {
            auto mul = (500000.0 / input.pulseWidth);       // multiplier in Hz
            input.pulseWidthIntegral = ((input.pulseWidthIntegral * mul) + input.pulseWidth) / (mul + 1);
        } else {
            input.pulseWidthIntegral = input.pulseWidth;
        }
        input.counter++;
    }
    else { // start or reset counters
        input.pulseWidthIntegral = 0;
        input.pulseWidth = 0;
        input.counter = 0;
    }
    input.lastPulse = micros;
    input.timeout = millis() + timeout;

    // _debug_printf_P(PSTR("Sensor_HLW8012::_calcPulseWidth: width %u integral %f counter %u\n"), input.pulseWidth, input.pulseWidthIntegral, input.counter);
}

#endif
