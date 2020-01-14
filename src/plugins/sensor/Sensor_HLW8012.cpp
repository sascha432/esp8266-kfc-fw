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

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF()
{
    auto tmp = micros();
    _interruptBufferCF.push_back(tmp);
    energyCounter++;
}

void ICACHE_RAM_ATTR Sensor_HLW8012_callbackCF1()
{
    auto tmp = micros();
    _interruptBufferCF1.push_back(tmp);
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

#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
    _noiseLevel = 0;
#endif

    _inputCF1 = &_inputCFI;
    digitalWrite(_pinSel, IOT_SENSOR_HLW8012_SEL_CURRENT);

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
        if (_inputCF.pulseWidthIntegral && IOT_SENSOR_HLW80xx_NO_NOISE(_noiseLevel)) {
            _inputCF.setTarget(_inputCF.pulseWidthIntegral);
        }
        // add energy counter
        noInterrupts();
        auto tempCounter = energyCounter;
        energyCounter = 0;
        interrupts();
        if (IOT_SENSOR_HLW80xx_NO_NOISE(_noiseLevel)) {
            _incrEnergyCounters(tempCounter);
        }

        _inputCF.toggleTimer = millis() + (60 * 1000); // set timeout for next pulse
    }
    else if (_inputCF.toggleTimer && millis() > _inputCF.toggleTimer) {
        // no pulse = something went wrong
        _inputCF.clear();
        _inputCF.setTarget(NAN);
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
                if (IOT_SENSOR_HLW80xx_NO_NOISE(_noiseLevel)) {
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
}

bool Sensor_HLW8012::_processInterruptBuffer(InterruptBuffer &buffer, SensorInput &input)
{
    auto settings = input.getSettings();

    if (buffer.size() >= 2) {

#if IOT_SENSOR_HLW80xx_DATA_PLOT
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
        bool canSend = client && client->canSend();
#endif

        // tested up to 5KHz
        // jitter +-0.03Hz @ 500Hz

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

#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
            if (&input == &_inputCFI) {
                // calculate noise level of the HLW8012
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

                    if (!IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION(_noiseLevel)) { // set current and power to zero, values will be updated but not shown
                        _power = 0;
                        _current = 0;
                    }
                }
            }
#endif

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

#if IOT_SENSOR_HLW80xx_DATA_PLOT
            if (dataType && canSend) {
                _plotData.push_back(value);
                if (convertUnits) {
                    _plotData.push_back(input.convertPulse(diff));
                    _plotData.push_back(input.convertPulse(input.average));
                    _plotData.push_back(input.convertPulse(input.pulseWidthIntegral));
                }
                else {
                    _plotData.push_back(diff);
                    _plotData.push_back(input.average);
                    _plotData.push_back(input.pulseWidthIntegral);
                }
            }
#endif

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

#if IOT_SENSOR_HLW80xx_DATA_PLOT
        // send every 100ms or 400 data points to keep the packets small
        // required RAM = (data points * 4 + 32) * 8
        // tested up to 1KHz which works without dropping any packet with good WiFi
        if (dataType && ((millis() > _plotDataTime) || _plotData.size() > 400)) {
            _plotDataTime = millis() + 100;

            if (canSend) {        // drop any data if the queue is full
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

                auto wsBuffer = Http2Serial::getConsoleServer()->makeBuffer(_plotData.size() * sizeof(*_plotData.data()) + sizeof(header_t));
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
#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
                        _noiseLevel
#else
                        0.0f
#endif
                    };
                    buffer += sizeof(header_t);
                    memcpy(buffer, _plotData.data(), _plotData.size() * sizeof(*_plotData.data()));

                    client->binary(wsBuffer);
                }
            }
            // debug_printf("ram=%u data=%u queue=%u cansend=%u\n", ESP.getFreeHeap(), _plotData.size(), client->_messageQueue.length(), canSend);
            _plotData.clear();
        }
#endif
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
}

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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWMODE, "HLWMODE", "<u=voltage/i=current/c=cycle>[,<delay in ms>", "Set voltage or current mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(HLWCFG, "HLWCFG", "<params,params,...>", "Configure sensor inputs", "Display configuration");

void Sensor_HLW8012::atModeHelpGenerator()
{
    Sensor_HLW80xx::atModeHelpGenerator();

    auto name = SensorPlugin::getInstance().getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWMODE), name);
}

static void print_sensor_input_settings(Stream &serial, Sensor_HLW8012::SensorInput &input, bool newLine)
{
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

bool Sensor_HLW8012::atModeHandler(Stream &serial, const String &command, AtModeArgs &args)
{
    if (Sensor_HLW80xx::atModeHandler(serial, command, args)) {
        return true;
    }
    else {
        if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(HLWMODE))) {
            if (args.requireArgs(1, 2)) {
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

                serial.printf_P(PSTR("%s, delay %dms\n"), type, delay);
                _setOutputMode(mode, delay);
            }
            return true;
        }
        else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(HLWCFG))) {
            if (args.isQueryMode()) {
                serial.printf_P(PSTR("+%s="), command.c_str());
                print_sensor_input_settings(serial, _inputCF, false);
                print_sensor_input_settings(serial, _inputCFU, false);
                print_sensor_input_settings(serial, _inputCFI, true);
            }
            else if (args.requireArgs(6, 6)) {
                uint16_t n = 0;
                _inputCF.getSettings() = { (uint16_t)args.toInt(n++), (uint8_t)args.toInt(n++), };
                _inputCFU.getSettings() = { (uint16_t)args.toInt(n++), (uint8_t)args.toInt(n++), };
                _inputCFI.getSettings() = { (uint16_t)args.toInt(n++), (uint8_t)args.toInt(n++), };
            }
            return true;
        }
    }
    return false;
}
#endif

#endif
