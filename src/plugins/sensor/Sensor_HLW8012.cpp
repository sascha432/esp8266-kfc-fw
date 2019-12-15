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

// work around for scheduled interrupts crashing with higher frequencies
static Sensor_HLW8012 *sensor = nullptr;
static volatile unsigned long callbackTimeCF;
static volatile unsigned long callbackTimeCF1;
static volatile unsigned long energyCounter = 0;
static volatile uint8_t callbackFlag = 0;

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF() {
    callbackTimeCF = micros();
    callbackFlag |= 0x01;
    energyCounter++;
}

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF1() {
    callbackTimeCF1 = micros();
    callbackFlag |= 0x02;
}

void Sensor_HLW8012::loop() {
    if (sensor) {
        if (callbackFlag & 0x01) {
            callbackFlag &= ~0x01;
            sensor->_callbackCF(callbackTimeCF);
            sensor->_energyCounter = energyCounter;
        }
        if (callbackFlag & 0x02) {
            callbackFlag &= ~0x02;
            sensor->_callbackCF1(callbackTimeCF1);
        }
    }
}


Sensor_HLW8012::Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1) : Sensor_HLW80xx(name), _pinSel(pinSel), _pinCF(pinCF), _pinCF1(pinCF1) {
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW8012(): component=%p\n"), this);
#endif
    registerClient(this);
    _output = CURRENT;
    memset((void *)&_input, 0, sizeof(_input));

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

Sensor_HLW8012::~Sensor_HLW8012() {

    LoopFunctions::remove(Sensor_HLW8012::loop);
    sensor = nullptr;
    detachInterrupt(digitalPinToInterrupt(_pinCF));
    detachInterrupt(digitalPinToInterrupt(_pinCF1));
}


void Sensor_HLW8012::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("Power Monitor HLW8012" HTML_S(br)));
}

Sensor_HLW8012::SensorEnumType_t Sensor_HLW8012::getType() const {
    return HLW8012;
}

String Sensor_HLW8012::_getId(const __FlashStringHelper *type) {
    PrintString id(F("hlw8012_0x%04x"), (_pinSel << 10) | (_pinCF << 5) | (_pinCF1));
    if (type) {
        id.write('_');
        id.print(FPSTR(type));
    }
    return id;
}

void Sensor_HLW8012::_callbackCF(unsigned long micros) {
    _calcPulseWidth(_input[0], micros);
    if (_input[0].pulseWidth && _input[0].counter > IOT_SENSOR_HLW8012_CF1_MIN_COUNT) {
        _power = IOT_SENSOR_HLW80xx_CALC_P(_input[0].pulseWidthIntegral);
    }
    else {
        _power = NAN;
    }
    _energyCounter++;
}

void Sensor_HLW8012::_callbackCF1(unsigned long micros) {
    _calcPulseWidth(_input[1], micros);
    if (_input[1].pulseWidth && _input[1].counter > IOT_SENSOR_HLW8012_CF1_MIN_COUNT) {
        if (_output == CURRENT) {
            _current = IOT_SENSOR_HLW80xx_CALC_I(_input[1].pulseWidthIntegral);
        }
        else if (_output == VOLTAGE) {
            _voltage = IOT_SENSOR_HLW80xx_CALC_U(_input[1].pulseWidthIntegral);
        }
    }
    if (_input[1].counter > IOT_SENSOR_HLW8012_CF1_TOGGLE_COUNT) {
        memset(&_input[1], 0, sizeof(_input[1]));
        _input[1].timeout = millis() + IOT_SENSOR_HLW8012_TIMEOUT;
        if (_output == CURRENT) {
            _output = VOLTAGE;
        } else {
            _output = CURRENT;
        }
        digitalWrite(_pinSel, _output);
    }
}

void Sensor_HLW8012::_calcPulseWidth(SensorInput_t &input, unsigned long micros) {
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
    else {
        memset(&input, 0, sizeof(input));
    }
    input.lastPulse = micros;
    input.timeout = millis() + IOT_SENSOR_HLW8012_TIMEOUT;

    // _debug_printf_P(PSTR("Sensor_HLW8012::_calcPulseWidth: width %u integral %f counter %u\n"), input.pulseWidth, input.pulseWidthIntegral, input.counter);
}

#endif
