/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && IOT_SENSOR_HAVE_HLW8012

#include <LoopFunctions.h>
#include "Sensor_HLW8012.h"
#include "sensor.h"
#include "MQTTSensor.h"
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
static volatile uint32_t skippedCFCounter = 0;
static volatile uint32_t skippedCF1Counter = 0;

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF()
{
    callbackTimeCF = micros();
    if (callbackFlag & CBF_CF) {
        callbackFlag |= CBF_CF_SKIPPED;
        skippedCFCounter++;
    }
    callbackFlag |= CBF_CF;
    energyCounter++;
}

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF1()
{
    callbackTimeCF1 = micros();
    if (callbackFlag & CBF_CF1) {
        callbackFlag |= CBF_CF1_SKIPPED;
        skippedCF1Counter++;
    }
    callbackFlag |= CBF_CF1;
}

void Sensor_HLW8012::loop()
{
    if (sensor) {
        sensor->_loop();
    }
}

void Sensor_HLW8012::dump(Print &output)
{
    output.printf_P(PSTR("CF/CF1 skipped=%u/%u CF/CF1 pulse=%u (%.0f) (%c) / %u (%.0f) - "),
        skippedCFCounter, skippedCF1Counter,
        _inputCF.pulseWidth, (float)_inputCF.pulseWidthIntegral,
        _output == VOLTAGE ? 'U' : 'I', _inputCF1.pulseWidth, (float)_inputCF1.pulseWidthIntegral
    );
    Sensor_HLW80xx::dump(output);
#if IOT_SENSOR_HLW8012_SENSOR_STATS
    _stats[0].dump(output);
    output.println();
    _stats[1].dump(output);
    output.println();
    _stats[2].dump(output);
    output.println();

    _stats[0].clear();
    _stats[1].clear();
    _stats[2].clear();
#endif
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
        _inputCF = SensorInput();
        _power = 0;
    }

    if (millis() > _inputCF1.timeout) {
        _inputCF1 = SensorInput();
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
#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP
    if (_rawDumpEndTime) {
        if (millis() > _rawDumpEndTime) {
            _endRawDataDump();
        }
    }
#endif
}

// ------------------------------------------------------------------------
// Sensor class
// ------------------------------------------------------------------------

Sensor_HLW8012::Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1) : Sensor_HLW80xx(name), _pinSel(pinSel), _pinCF(pinCF), _pinCF1(pinCF1)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW8012(): component=%p\n"), this);
#endif
#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP
    _rawDumpEndTime = 0;
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

#if IOT_SENSOR_HLW8012_SENSOR_STATS
    _stats[0].type = 'U';
    _stats[1].type = 'I';
    _stats[2].type = 'P';
#endif

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
#if IOT_SENSOR_HLW8012_SENSOR_STATS
    _stats[2].updateTime();
    _stats[2].add(_inputCF.pulseWidth, _inputCF.pulseWidthIntegral, _inputCF.counter);
#endif
    if (_inputCF.pulseWidth && _inputCF.counter >= 2) {
        _power = IOT_SENSOR_HLW80xx_CALC_P(_inputCF.pulseWidthIntegral);
    }
    else {
        _power = 0;
    }
#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP
    if (_rawDumpEndTime) {
        _rawDumpFile.printf("cf,%u,%u,%u,%u,%.2f\n", skippedCFCounter, (uint32_t)micros, (uint32_t)_inputCF.lastPulse, (uint32_t)_inputCF.pulseWidth, _inputCF.pulseWidthIntegral);
    }
#endif
    // _debug_printf_P(PSTR("pulse width %f count %u power %f energy %u\n"), _inputCF.pulseWidthIntegral, _inputCF.counter, IOT_SENSOR_HLW80xx_CALC_P(_inputCF.pulseWidthIntegral), _energyCounter);
}

void Sensor_HLW8012::_callbackCF1(unsigned long micros)
{
    if (millis() < _inputCF1.delayStart) { // skip anything before to let sensor settle
        return;
    }

    _calcPulseWidth(_inputCF1, micros, _output == VOLTAGE ? IOT_SENSOR_HLW8012_TIMEOUT_U : IOT_SENSOR_HLW8012_TIMEOUT_I);
#if IOT_SENSOR_HLW8012_SENSOR_STATS
    _stats[_output].updateTime();
    _stats[_output].add(_inputCF1.pulseWidth, _inputCF1.pulseWidthIntegral, _inputCF1.counter);
#endif

    if (_inputCF1.pulseWidth) {
        // update values if enough pulses have been counted
        if (_output == CURRENT && _inputCF1.counter >= 3) {
            _current = IOT_SENSOR_HLW80xx_CALC_I(_inputCF1.pulseWidthIntegral);
        }
        else if (_output == VOLTAGE && _inputCF1.counter >= 10) {
            _voltage = IOT_SENSOR_HLW80xx_CALC_U(_inputCF1.pulseWidthIntegral);
        }
    }
#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP
    if (_rawDumpEndTime) {
        _rawDumpFile.printf("cf1,%c,%u,%u,%u,%u,%.2f\n", _output == VOLTAGE ? 'u' : 'i', skippedCF1Counter, (uint32_t)micros, (uint32_t)_inputCF1.lastPulse, (uint32_t)_inputCF1.pulseWidth, _inputCF1.pulseWidthIntegral);
    }
#endif
    if (millis() > _inputCF1.toggleTimer) { // toggle voltage/current measurement

        if (_output == CURRENT) {
            _debug_printf_P(PSTR("pulse width %f count %u current %f / %f energy %u\n"), _inputCF1.pulseWidthIntegral, _inputCF1.counter, IOT_SENSOR_HLW80xx_CALC_I(_inputCF1.pulseWidthIntegral), _current, (uint32_t)_energyCounter[0]);
        }
        else {
            _debug_printf_P(PSTR("pulse width %f count %u voltage %f / %f energy %u\n"), _inputCF1.pulseWidthIntegral, _inputCF1.counter, IOT_SENSOR_HLW80xx_CALC_U(_inputCF1.pulseWidthIntegral), _voltage, (uint32_t)_energyCounter[0]);
        }

        _toggleOutputMode();
    }
}

void Sensor_HLW8012::_setOutputMode(CF1OutputTypeEnum_t outputMode)
{
    _debug_printf_P(PSTR("Sensor_HLW8012::_setOutputMode(): mode=%u\n"), outputMode);
    if (outputMode == VOLTAGE) {
        _output = CURRENT;
    }
    else if (outputMode == CURRENT) {
        _output = VOLTAGE;
    }
    _toggleOutputMode();
    if (outputMode != CYCLE) { // lock selected mode
        _inputCF1.toggleTimer = ~0;
    }
}

void Sensor_HLW8012::_toggleOutputMode()
{
    _inputCF1 = SensorInput();
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
#if IOT_SENSOR_HLW8012_SENSOR_STATS
    _stats[_output].clear();
#endif
}

void Sensor_HLW8012::_calcPulseWidth(SensorInput &input, unsigned long micros, uint16_t timeout)
{
    if (input.lastPulse && millis() < input.timeout) {
        input.pulseWidth = get_time_diff(input.lastPulse, micros);
        if (input.lastIntegration) {
            // integrate with 1Hz
            auto time = get_time_diff(input.lastIntegration, micros); // we keep the time separate from last pulse in case we skipped pulses
            auto mul = (500000.0 / time);
            input.pulseWidthIntegral = ((input.pulseWidthIntegral * mul) + input.pulseWidth) / (mul + 1.0);
        } else {
            input.pulseWidthIntegral = input.pulseWidth;
        }
        input.lastIntegration = micros;
        input.counter++;
    }
    else { // start or reset counters
        input.pulseWidthIntegral = 0;
        input.pulseWidth = 0;
        input.counter = 0;
        input.lastIntegration = 0;
    }
    input.lastPulse = micros;
    input.timeout = millis() + timeout;

    // _debug_printf_P(PSTR("Sensor_HLW8012::_calcPulseWidth: width %u integral %f counter %u\n"), input.pulseWidth, input.pulseWidthIntegral, input.counter);
}

#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP

Sensor_HLW8012 *Sensor_HLW8012::_getFirstSensor()
{
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == HLW8012) {
            return reinterpret_cast<Sensor_HLW8012 *>(sensor);
        }
    }
    return nullptr;
}

void Sensor_HLW8012::_simulateCallbackCF1(uint32_t interval)
{
    static EventScheduler::TimerPtr timer = nullptr;
    _debug_printf_P(PSTR("Sensor_HLW8012::_simulateCallbackCF1(): interval=%u, old timer=%p\n"), interval, timer);

    Scheduler.removeTimer(&timer);
    if (interval) {
        Scheduler.addTimer(&timer, interval, true, [](EventScheduler::TimerPtr) {
            Sensor_HLW8012_callbackCF1();
        }, EventScheduler::PRIO_HIGH);
    }
}

bool Sensor_HLW8012::_startRawDataDump(const String &filename, uint32_t endTime)
{
    // end current dump if any is running
    _endRawDataDump();

    _debug_printf_P(PSTR("Sensor_HLW8012::_startRawDataDump(): filename=%s, endTime=%u\n"), filename.c_str(), endTime);

    _rawDumpFile = SPIFFS.open(filename, fs::FileOpenMode::write);
    if (_rawDumpFile) {
        _rawDumpEndTime = endTime;
        _debug_printf_P(PSTR("Sensor_HLW8012::_startRawDataDump(): started, file=%u\n"), (bool)_rawDumpFile);
        return true;
    }
    else {
        _rawDumpEndTime = 0;
        _debug_printf_P(PSTR("Sensor_HLW8012::_startRawDataDump(): failed to create %s\n"), filename.c_str());
    }
    return false;
}

void Sensor_HLW8012::_endRawDataDump()
{
    if (_rawDumpFile) {
        _debug_printf_P(PSTR("Sensor_HLW8012::_endRawDataDump(): raw data dump ended: size=%u\n"), (unsigned)_rawDumpFile.size());
        _rawDumpFile.close();
    }
    _rawDumpEndTime = 0;
}

#endif

#endif
