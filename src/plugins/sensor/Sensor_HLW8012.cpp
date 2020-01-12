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
static volatile uint32_t energyCounter = 0;
static Sensor_HLW8012::InterruptBuffer _interruptBufferCF;
static Sensor_HLW8012::InterruptBuffer _interruptBufferCF1;

#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
static Sensor_HLW8012::TimeBuffer _timeCFbuffer;
static Sensor_HLW8012::TimeBuffer _timeCF1buffer;
#endif

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF()
{
    auto tmp = micros();
    _interruptBufferCF.push_back(tmp);
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    _timeCFbuffer.push_back_no_block(tmp);
#endif
    energyCounter++;
}

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF1()
{
    auto tmp = micros();
    _interruptBufferCF1.push_back(tmp);
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    _timeCF1buffer.push_back_no_block(tmp);
#endif
}

// ------------------------------------------------------------------------
// Sensor class
// ------------------------------------------------------------------------

Sensor_HLW8012::Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1) :
    Sensor_HLW80xx(name),
    _pinSel(pinSel),
    _pinCF(pinCF),
    _pinCF1(pinCF1),
    _inputCF(_power),
    _inputCFI(_current),
    _inputCFU(_voltage)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW8012(): component=%p\n"), this);
#endif
    registerClient(this);

    _inputCF.setCallback([this](double pulseWidth) {
        return (float)IOT_SENSOR_HLW80xx_CALC_P(pulseWidth);
    });
    _inputCFU.setCallback([this](double pulseWidth) {
        return (float)IOT_SENSOR_HLW80xx_CALC_U(pulseWidth);
    });
    _inputCFI.setCallback([this](double pulseWidth) {
        return (float)IOT_SENSOR_HLW80xx_CALC_I(pulseWidth);
    });
    // auto &settings = _inputCFU.getSettings();
    // settings.avgValCount = 5;
    // settings = _inputCF.getSettings();
    // settings.avgValCount = 3;

    _noiseLevel = 0;

    _inputCF1 = &_inputCFI;
    digitalWrite(_pinSel, IOT_SENSOR_HLW8012_SEL_CURRENT);

    pinMode(_pinSel, OUTPUT);
    pinMode(_pinCF, INPUT);
    pinMode(_pinCF1, INPUT);

    if (sensor) {
        __debugbreak_and_panic_printf_P(PSTR("Only one instance of Sensor_HLW8012 supported\n"));
    }

#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    _autoDumpCount = -1;
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


void Sensor_HLW8012::loop()
{
    if (sensor) {
        sensor->_loop();
    }
}

void Sensor_HLW8012::_loop()
{
    // power and energy
    if (_processInterruptBuffer(_interruptBufferCF, _inputCF)) {
        // debug_printf("_inputCF: i=%f i2=%f c=%u avg=%u d=%u\n",_inputCF.pulseWidthIntegral,_inputCF.pulseWidthIntegral2,_inputCF.counter,_inputCF.average,_inputCF.delayStart);
        if (_inputCF.pulseWidthIntegral && _noiseLevel < IOT_SENSOR_HLW80xx_MAX_NOISE) {
            _inputCF.setTarget(_inputCF.pulseWidthIntegral);
        }
        // add energy counter
        noInterrupts();
        auto tempCounter = energyCounter;
        energyCounter = 0;
        interrupts();
        _incrEnergyCounters(tempCounter);

        _inputCF.toggleTimer = millis() + (60 * 1000); // set timeout for next pulse
    }
    else if (_inputCF.toggleTimer && millis() > _inputCF.toggleTimer) {
        // no pulse = something went wrong
        _inputCF.clear();
        _inputCF.setTarget(0);
    }
    if (millis() > _saveEnergyCounterTimeout) {
        _saveEnergyCounter();
    }

    // voltage and current
    if (_inputCF1->delayStart) {
        if (millis() > _inputCF1->delayStart) {
            _inputCF1->delayStart = 0;
            _inputCF1->counter = 1;
            noInterrupts();
            _interruptBufferCF1.clear();
            interrupts();
        }
    }
    else if (_processInterruptBuffer(_interruptBufferCF1, *_inputCF1)) {
        if (_inputCF1->pulseWidthIntegral) {
            if (_inputCF1 == &_inputCFI) {
                if (_noiseLevel < IOT_SENSOR_HLW80xx_MAX_NOISE) {
                    _inputCF1->setTarget(_inputCF1->pulseWidthIntegral);
                }
            }
            else {
                _inputCF1->setTarget(_inputCF1->pulseWidthIntegral);
            }
        }
    }
    // toggle between voltage/current
    if (millis() > _inputCF1->toggleTimer) {
        _debug_printf_P(PSTR("toggleTimer=%c counter=%u\n"), _getOutputMode(_inputCF1), _inputCF1->counter);
        if (_inputCF1->counter == 0) {
            _inputCF1->setTarget(NAN);
        }
        _toggleOutputMode();
    }

#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    if (_autoDumpCount != -1) {
        if ((int)_timeCF1buffer.getCount() >= _autoDumpCount) {
            MySerial.printf_P(PSTR("+SP_HLWBUF: Auto dump count=%u\n"), _autoDumpCount);
            _autoDumpCount = -1;
            _dumpBufferInfo(MySerial, _timeCF1buffer, F("CF1"));
            _dumpBuffer(MySerial, _inputCF1, -1);
        }
    }
#endif
}

bool Sensor_HLW8012::_processInterruptBuffer(InterruptBuffer &buffer, SensorInput &input)
{
    auto settings = input.getSettings();

    if (buffer.size() >= 2) {

        auto client = _getWebSocketClient();
        bool convertUnits = _getWebSocketPlotData() & WebSocketDataTypeEnum_t::CONVERT_UNIT;
        uint16_t dataType = 0;
        if (client) {
            if ((_getWebSocketPlotData() & WebSocketDataTypeEnum_t::CURRENT) && (&input == &_inputCFI)) {
                dataType = 'I';
            }
            else if ((_getWebSocketPlotData() & WebSocketDataTypeEnum_t::POWER) && (&input == &_inputCF)) {
                dataType = 'P';
            }
            else if ((_getWebSocketPlotData() & WebSocketDataTypeEnum_t::VOLTAGE) && (&input == &_inputCFU)) {
                dataType = 'U';
            }
        }

        // tested up to 5KHz
        // jitter +-0.03Hz @ 500Hz

        std::vector<float> data;

#define USE_POPFRONT 0

#if USE_POPFRONT
        noInterrupts();
        auto iterator = buffer.begin();
        auto lastValue = *iterator;
        while(++iterator != (buffer.end() - 1)) {
            auto value = buffer.pop_front();
            interrupts();
#else
        auto iterator = buffer.begin();
        auto lastValue = *iterator;
        while(++iterator != buffer.end()) {
            auto value = *iterator;
#endif
            auto diff = __inline_get_time_diff(lastValue, value);
            lastValue = value;

            if (&input == &_inputCFI) {
                // calculate noise level of the HLW8012, according to the data sheet the CSE7759 has it builtin and sends a 2Hz signal
                uint32_t noiseMin = ~0;
                uint32_t noiseMax = 0;
                uint32_t noiseSum = 0;
                _noiseBuffer.push_back(diff);
                if (_noiseBuffer.size() >= _noiseBuffer.capacity())  {
                    for(auto noiseDiff: _noiseBuffer) {
                        noiseMin = min(noiseMin, noiseDiff);
                        noiseMax = max(noiseMax, noiseDiff);
                        noiseSum += noiseDiff;
                    }
                    float noiseMean = noiseSum / _noiseBuffer.size();
                    uint32_t noiseLevel = ((noiseMean - noiseMin) + (noiseMax - noiseMean)) * 50000.0 / noiseMean; // in %%

                    // filter quick changes in current which looks like noise
                    uint32_t multiplier = 500 * 2000 / diff;
                    _noiseLevel = ((_noiseLevel * multiplier) + noiseLevel) / (multiplier + 1.0);

                    // if (_noiseLevel > 2000 && noiseMean > IOT_SENSOR_HLW80xx_SHUNT_NOISE_PULSE) {
                    //     // could still be on, display some arbitrary low values
                    //     _current = 0.01;
                    // }
                    //else
                    if (_noiseLevel >= IOT_SENSOR_HLW80xx_MAX_NOISE) { // set current and power to zero, values will be updated but not shown
                        _power = 0;
                        _current = 0;
                    }
                }
            }

            if (input.counter == 0 || settings.avgValCount == 0) {
                input.average = diff;
            }
            else {
                // get an average of the last N values to filter spikes
                uint32_t multiplier = std::min((uint32_t)settings.avgValCount, input.counter);
                input.average = ((input.average * multiplier) + diff) / (multiplier + 1);
            }
            input.counter++;

            // use average for the integration
            auto diff2 = input.average;
            if (input.pulseWidthIntegral) {
                uint32_t multiplier = 500 * settings.intTime / input.lastIntegration;
                input.pulseWidthIntegral = ((input.pulseWidthIntegral * multiplier) + diff2) / (multiplier + 1.0);
                input.lastIntegration = diff2;
            }
            else {
                input.pulseWidthIntegral = diff2;
                input.lastIntegration = diff2;
            }

            if (dataType) {
                data.push_back(value);
                if (convertUnits) {
                    data.push_back(input.convertPulse(diff));
                    data.push_back(input.convertPulse(input.average));
                    data.push_back(input.convertPulse(input.pulseWidthIntegral));
                }
                else {
                    data.push_back(diff);
                    data.push_back(input.average);
                    data.push_back(input.pulseWidthIntegral);
                }
            }

#if USE_POPFRONT
            noInterrupts();
        }
        interrupts();
#else
    }
#endif

#if !USE_POPFRONT
        // copy last value and items that have not been processed, mostly 1 item...
        noInterrupts(); // takes ~20-30µs
        buffer = std::move(buffer.slice(--iterator, buffer.end()));
        interrupts();
#endif


        if (dataType) {

            typedef struct {
                uint16_t packetId;
                uint16_t outputMode;
                uint16_t dataType;
                float voltage;
                float current;
                float power;
                float energy;
                float pf;
                float noise;
            } header_t;

            auto wsBuffer = Http2Serial::getConsoleServer()->makeBuffer(data.size() * sizeof(*data.data()) + sizeof(header_t));
            auto buffer = wsBuffer->get();
            if (buffer) {
                header_t *header = reinterpret_cast<header_t *>(buffer);
                *header = {
                    0x0100,
                    _getOutputMode(&input),
                    dataType,
                    _voltage,
                    _current,
                    _power,
                    _getEnergy(0),
                    _getPowerFactor(),
                    _noiseLevel
                };
                buffer += sizeof(header_t);
                memcpy(buffer, data.data(), data.size() * sizeof(*data.data()));

                client->binary(wsBuffer);
            }
        }
        return true;
    }
    return false;
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

void Sensor_HLW8012::_setOutputMode(OutputTypeEnum_t outputMode, int delay)
{
    _debug_printf_P(PSTR("Sensor_HLW8012::_setOutputMode(): mode=%u\n"), outputMode);
    if (outputMode == VOLTAGE) {
        _inputCF1 = &_inputCFI;
    }
    else if (outputMode == CURRENT) {
        _inputCF1 = &_inputCFU;
    }
    _toggleOutputMode(delay);
    if (outputMode != CYCLE) { // lock selected mode
        _inputCF1->toggleTimer = ~0;
    }
}

void Sensor_HLW8012::_toggleOutputMode(int delay)
{
    if (_inputCF1 == &_inputCFI) {
        _inputCF1 = &_inputCFU;
        _inputCFU.delayStart = millis() + (delay == -1 ? IOT_SENSOR_HLW8012_DELAY_START_U : delay);
        _inputCFU.toggleTimer = _inputCFU.delayStart + IOT_SENSOR_HLW8012_MEASURE_LEN_U;
        digitalWrite(_pinSel, IOT_SENSOR_HLW8012_SEL_VOLTAGE);
    } else {
        _inputCF1 = &_inputCFI;
        _inputCFI.delayStart = millis() + (delay == -1 ? IOT_SENSOR_HLW8012_DELAY_START_I : delay);
        _inputCFI.toggleTimer = _inputCFI.delayStart + IOT_SENSOR_HLW8012_MEASURE_LEN_I;
        digitalWrite(_pinSel, IOT_SENSOR_HLW8012_SEL_CURRENT);
    }

#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    noInterrupts();
    _timeCF1buffer.clear();
    interrupts();
#endif
}

#if IOT_SENSOR_HLW8012_BUFFER_DEBUG


#define IOT_SENSOR_HLW8012_INTEGRAL_UPDATE_RATE 1.0

void Sensor_HLW8012::_dumpBufferInfo(Stream &output, Sensor_HLW8012::TimeBuffer &buffer, const String &name)
{
    float start = get_time_diff(buffer.front(), micros()) / 1000000.0;
    uint32_t dur = 0;
    float avg = NAN;
    if (buffer.size() > 1) {
        auto lockStart = micros();
        buffer.lock();
        auto lastValue = buffer.front();
        auto iterator = buffer.begin();
        uint32_t sum = 0;
        while(++iterator != buffer.end()) {
            sum += *iterator - lastValue;
            lastValue = *iterator;
        }
        avg = sum / buffer.size() / 1000.0;
        buffer.unlock();
        dur = micros() - lockStart;
    }
    output.printf_P(PSTR("Buffer %s: count=%u time=%.4fsec avg=%.2fms %.2fHz (%uµs)\n"), name.c_str(), buffer.getCount(), start, avg, 500.0 / avg, dur);
}

void Sensor_HLW8012::_autoDumpBuffer(Stream &output, int count)
{
    noInterrupts();
    _timeCF1buffer.clear();
    interrupts();
    _autoDumpCount = count;
}

void Sensor_HLW8012::_dumpBuffer(Stream &output, SensorInput *input, int limit)
{
    if (!input) {
        _dumpBufferInfo(output, _timeCFbuffer, F("CF"));
        _dumpBufferInfo(output, _timeCF1buffer, F("CF1"));
        return;
    }

    TimeBuffer &list = (input == &_inputCF) ? _timeCFbuffer : _timeCF1buffer;

    if (list.getCount()) {
        output.printf("BUFFER %u: ", list.getCount());
        auto iterator = list.begin();
        list.lock();
        auto s1 = micros();
        auto lastValue = list.front();
        iterator = list.begin();
        float integral = 0;
        uint32_t sum = 0;
        uint16_t count = 0;
        uint32_t lastDiff = 0;
        while(++iterator != list.end()) {
            auto diff = get_time_diff(lastValue, *iterator);
            lastValue = *iterator;
            sum += diff;
            count++;
            if (integral == 0) {
                integral = diff;
                lastDiff = diff;
            }
            else {
                auto mul = (500000.0 / lastDiff) * IOT_SENSOR_HLW8012_INTEGRAL_UPDATE_RATE;
                integral = ((integral * mul) + diff) / (mul + 1.0);
                lastDiff = diff;
            }
        }
        auto s2 = micros();

        if (limit != 0) {
            iterator = list.begin();
            if (limit != -1) {
                int skip = list.size() - limit;
                if (skip > 0) {
                    iterator += skip;
                }
            }
            lastValue = *iterator;
            auto first = list.front();
            while(++iterator != list.end()) {
                auto diff = get_time_diff(lastValue, *iterator);
                lastValue = *iterator;
                output.printf("%u(%.2fs) ", diff, (lastValue - first) / 1000000.0);
            }
            output.println();
        }

        float fCount = count;
        output.printf_P(PSTR("processing time=%luµs count=%u integral=%f avg=%f %c=%.4f %.4f %f list=%u/%u\n"),
            s2 - s1, count, integral, sum / fCount,
            _getOutputMode(input),
            input->convertPulse(integral),
            input->convertPulse(sum / fCount),
            input->getTarget(), list.getCount(), list.size()
        );
        noInterrupts();
        list.clear();
        interrupts();
    }
}

#endif

void Sensor_HLW8012::dump(Print &output)
{
    output.printf_P(PSTR("Pi=% 8.3fms avg=% 8.3fms cnt=% 4u %ci=% 9.4fms avg=% 9.4f cnt=% 4u "),
        _inputCF.pulseWidthIntegral / 1000.0,
        _inputCF.average / 1000.0,
        _inputCF.counter,
        _getOutputMode(_inputCF1),
        _inputCF1->pulseWidthIntegral / 1000.0,
        _inputCF1->average / 1000.0,
        _inputCF1->counter
    );
    Sensor_HLW80xx::dump(output);
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX         "SP_"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWMODE, "HLWMODE", "<u=0/i=1>[,<delay in ms>,<count=auto dump buffer/0=off>]", "Set voltage or current mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(HLWCFG, "HLWCFG", "<params,params,...>", "Configure sensor", "Display configuration");
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
PROGMEM_AT_MODE_HELP_COMMAND_DEF(HLWBUF, "HLWBUF", "<U/I/P>", "Dump buffer for voltage, current or power", "Display buffer status");
#endif

void Sensor_HLW8012::atModeHelpGenerator()
{
    Sensor_HLW80xx::atModeHelpGenerator();

    auto name = SensorPlugin::getInstance().getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWMODE), name);
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWBUF), name);
#endif
}

bool Sensor_HLW8012::atModeHandler(Stream &serial, const String &command, AtModeArgs &args)
{
    if (!Sensor_HLW80xx::atModeHandler(serial, command, args)) {
        if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(HLWMODE))) {
            if (args.requireArgs(1)) {
                auto type = F("Cycle Voltage/Current");
                Sensor_HLW8012::OutputTypeEnum_t mode = Sensor_HLW8012::OutputTypeEnum_t::CYCLE;

                at_mode_print_prefix(serial, command);
                serial.print(F("Setting CF1 output mode to "));
                char ch = args.toLowerChar(0);
                if (ch == 'u') {
                    type = F("Voltage");
                    mode = Sensor_HLW8012::OutputTypeEnum_t::VOLTAGE;
                }
                else if (ch == 'i') {
                    type = F("Current");
                    mode = Sensor_HLW8012::OutputTypeEnum_t::CURRENT;
                }
                auto delay = (int)args.toInt(1, -1);
                auto autoDump = args.toInt(2);

                serial.printf_P(PSTR("%s for first sensor, delay %dms, auto dump=%u\n"), type, delay, autoDump);
                _setOutputMode(mode, delay);
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
                if (autoDump) {
                    _autoDumpBuffer(serial, autoDump);
                }
#endif
            }
            return true;
        }
        if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(HLWCFG))) {
            if (args.isQueryMode()) {
                auto print = [](Stream &serial, SensorInput &input, bool newLine) {
                    auto settings = input.getSettings();
                    serial.printf_P(PSTR("%u,%u"), settings.intTime, settings.avgValCount);
                    if (newLine) {
                        serial.println();
                    }
                    else {
                        serial.print(',');
                        serial.print(' ');
                    }
                };

                serial.printf_P(PSTR("+%s="), command.c_str());
                print(serial, _inputCF, false);
                print(serial, _inputCFU, false);
                print(serial, _inputCFI, true);
            }
            else if (args.requireArgs(6, 6)) {
                uint16_t n = 0;
                _inputCF.getSettings() = { args.toInt(n++), args.toInt(n++), };
                _inputCFU.getSettings() = { args.toInt(n++), args.toInt(n++), };
                _inputCFI.getSettings() = { args.toInt(n++), args.toInt(n++), };
            }
            return true;
        }
#if IOT_SENSOR_HLW8012_BUFFER_DEBUG
        if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(HLWBUF))) {
            if (args.isQueryMode() || args.size() == 0) {
                _dumpBuffer(serial, nullptr);
            }
            else {
                auto type = args.toLowerChar(0);
                auto limit = args.toInt(1);
                if (type == 'u') {
                    _dumpBuffer(serial, &_inputCFU, limit);
                }
                else  if (type == 'i') {
                    _dumpBuffer(serial, &_inputCFI, limit);
                }
                else  if (type == 'p') {
                    _dumpBuffer(serial, &_inputCF, limit);
                }
            }
            return true;
        }
#endif
    }
    return false;
}
#endif

#endif
