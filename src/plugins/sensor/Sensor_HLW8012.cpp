/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_HLW8012

#include <EventScheduler.h>
#include "Sensor_HLW8012.h"
#include "sensor.h"
#include "MQTTSensor.h"
#include "MicrosTimer.h"
#include "web_socket.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

// ------------------------------------------------------------------------
// Low level interrupt handling
// ------------------------------------------------------------------------

static Sensor_HLW8012 *sensor = nullptr;
static volatile uint32_t energyCounter = 0;
static Sensor_HLW8012::InterruptBuffer _interruptBufferCF;
static Sensor_HLW8012::InterruptBuffer _interruptBufferCF1;

extern "C" void IRAM_ATTR Sensor_HLW8012_callbackCF()
{
    _interruptBufferCF.push_back(micros());
    energyCounter++;
}

extern "C" void IRAM_ATTR Sensor_HLW8012_callbackCF1()
{
    _interruptBufferCF1.push_back(micros());
}

// ------------------------------------------------------------------------
// Sensor class
// ------------------------------------------------------------------------

Sensor_HLW8012::Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1) :
    Sensor_HLW80xx(name, SensorType::HLW8012),
    _pinSel(pinSel),
    _pinCF(pinCF),
    _pinCF1(pinCF1),
    _inputCF(_power),
    _inputCFI(_current),
    _inputCFU(_voltage)
{
    REGISTER_SENSOR_CLIENT(this);

    if (sensor) {
        __DBG_panic("Only one instance of Sensor_HLW8012 supported");
    }

    _inputCF.setCallback([this](double pulseWidth) -> float {
        return IOT_SENSOR_HLW80xx_CALC_P(pulseWidth);
    });
    _inputCFU.setCallback([this](double pulseWidth) -> float {
        return IOT_SENSOR_HLW80xx_CALC_U(pulseWidth);
    });
    _inputCFI.setCallback([this](double pulseWidth) -> float {
        return IOT_SENSOR_HLW80xx_ADJ_I_CALC(_dimmingLevel, IOT_SENSOR_HLW80xx_CALC_I(pulseWidth));
    });
    // auto &settings = _inputCFU.getSettings();
    // settings.avgValCount = 5;
    // settings = _inputCF.getSettings();
    // settings.avgValCount = 3;

    #if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
        _noiseLevel = 0;
    #endif

    pinMode(_pinSel, OUTPUT);
    pinMode(_pinCF, INPUT);
    pinMode(_pinCF1, INPUT);

    __LDBG_printf("pin sel=%u cf=%u cf1=%u", _pinSel, _pinCF, _pinCF1);

    _inputCF1 = &_inputCFI;
    _toggleOutputMode();

    #if PIN_MONITOR_USE_GPIO_INTERRUPT
        #error interrupt mode not compatible with arduino attachInterrupt
    #endif

    // singleton
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
    UNREGISTER_SENSOR_CLIENT(this);
}


void Sensor_HLW8012::loop()
{
    sensor->_loop();
}

void Sensor_HLW8012::_loop()
{
    // power and energy
    if (_processInterruptBuffer(_interruptBufferCF, _inputCF)) {
        if (_inputCF.pulseWidthIntegral && IOT_SENSOR_HLW80xx_NO_NOISE(_noiseLevel)) {
            _inputCF.setTarget(_inputCF.pulseWidthIntegral);
        }
        // add energy counter
        decltype(energyCounter) tempCounter;
        {
            InterruptLock lock;
            tempCounter = std::exchange(energyCounter, 0);
        }
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
            InterruptLock lock;
            _interruptBufferCF1.clear();
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
        __LDBG_printf("toggleTimer=%c counter=%u", _getOutputMode(_inputCF1), _inputCF1->counter);
        if (_inputCF1->counter == 0) {
            _inputCF1->setTarget(NAN);
        }
        _toggleOutputMode();
    }
}

float calculateSD(float data[])
{
    float sum = 0.0, mean, standardDeviation = 0.0;
    for(int i = 0; i < 10; i++) {
        sum += data[i];
    }
    mean = sum / 10;
    for(int i = 0; i < 10; i++) {
        standardDeviation += pow(data[i] - mean, 2);
    }
    return sqrt(standardDeviation / 10);
}

bool Sensor_HLW8012::_processInterruptBuffer(InterruptBuffer &buffer, SensorInput &input)
{
    auto settings = input.getSettings();

    size_t size;
    {
        InterruptLock lock;
        size = buffer.size();
    }
    if (size >= 2) {

        #if IOT_SENSOR_HLW80xx_DATA_PLOT
            auto client = Http2Serial::getClientById(_webSocketClient);
            bool convertUnits = false;
            uint16_t dataType = 0;
            if (client) {
                convertUnits = _getWebSocketPlotData() & WebSocketDataTypeEnum_t::CONVERT_UNIT;
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
        #endif

        // tested up to 5KHz
        // jitter +-0.03Hz @ 500Hz
        {
            InterruptLock lock;
            auto iterator = buffer.begin();
            auto lastValue = *iterator;
            while(++iterator != buffer.end()) {
                auto value = *iterator;
                lock.~InterruptLock();

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
                            float standardDeviation = 0.0;
                            for(auto noiseDiff: _noiseBuffer) {
                                noiseMin = min(noiseMin, noiseDiff);
                                noiseMax = max(noiseMax, noiseDiff);
                                noiseSum += noiseDiff;
                            }
                            float noiseMean = noiseSum / _noiseBuffer.size();
                            for(auto noiseDiff: _noiseBuffer) {
                                standardDeviation += pow(noiseMean, 2);
                            }
                            standardDeviation = sqrt(standardDeviation / _noiseBuffer.size());
                            uint32_t noiseLevel = ((noiseMean - noiseMin) + (noiseMax - noiseMean)) * 50000.0 / noiseMean; // in %%

                            // filter quick changes in current which looks like noise
                            uint32_t multiplier = 500 * 2000 / diff;
                            _noiseLevel = ((_noiseLevel * multiplier) + noiseLevel) / (multiplier + 1.0);

                            //TODO check standardDeviation
                            debug_printf_P(PSTR("sd %f lvl %f\n"), standardDeviation, _noiseLevel / 10);

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
                    if (dataType) {
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

            new(static_cast<void *>(&lock)) InterruptLock(); // recreate destroyed lock
        }

        // copy last value and items that have not been processed
        --iterator;
        // buffer = std::move(buffer.slice(iterator, buffer.end()));           // takes ~20-30µs
        buffer.shrink(iterator, buffer.end());                              // <1µs
    }

#if IOT_SENSOR_HLW80xx_DATA_PLOT
        // send every 100ms or 400 data points to keep the packets small
        // required RAM = (data points * 4 + 32) * 8
        // tested up to 1KHz which works without dropping any packet with good WiFi
        if (dataType && ((millis() > _plotDataTime) || _plotData.size() > 400)) {
            _plotDataTime = millis() + 100;

            if (client->canSend()) { // drop data if the queue is full
                typedef struct {
                    WSClient::BinaryPacketType type;
                    uint16_t outputMode;
                    uint16_t dataType;
                    float voltage;
                    float current;
                    float power;
                    float energy;
                    float pf;
                    float noise;
                    float level;
                } header_t;

                auto wsBuffer = Http2Serial::getServerSocket()->makeBuffer(_plotData.size() * sizeof(*_plotData.data()) + sizeof(header_t));
                auto buffer = wsBuffer->get();
                if (buffer) {
                    header_t *header = reinterpret_cast<header_t *>(buffer);
                    *header = {
                        WSClient::BinaryPacketType::HLW8012_PLOT_DATA,
                        _getOutputMode(&input),
                        dataType,
                        _voltage,
                        _current,
                        _power,
                        _getEnergy(0),
                        _getPowerFactor(),
                        #if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
                            _noiseLevel,
                        #else
                            0.0f,
                        #endif
                        #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
                            _dimmingLevel
                        #else
                            -1.0f
                        #endif
                    };
                    buffer += sizeof(header_t);
                    memcpy(buffer, _plotData.data(), _plotData.size() * sizeof(*_plotData.data()));

                    client->binary(wsBuffer);
                }
            }
            _plotData.clear();
        }
#endif
        return true;
    }
    return false;
}


void Sensor_HLW8012::getStatus(Print &output)
{
    output.printf_P(PSTR("Power Monitor HLW8012" HTML_S(br) "Calibration U=%f, I=%f, P=%f, Rs="), _calibrationU, _calibrationI, _calibrationP);
    printTrimmedFloat(&output, IOT_SENSOR_HLW80xx_SHUNT, 5);
    output.print(F("R" HTML_S(br)));

    #if DEBUG_IOT_SENSOR
        output.printf_P(PSTR(HTML_S(br) "PINS: sel=%u cf=%u cf1=%u"), _pinSel, _pinCF, _pinCF1);
    #endif
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
    __LDBG_printf("mode=%u", outputMode);
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
    }
    else {
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
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "SP_"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWCAL, "HLWCAL", "<u=voltage/i=current/p=power>[,<repeat>]|[,<displayed value>,<real value>]", "Enter calibration mode or set values");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWMODE, "HLWMODE", "<u=voltage/i=current/c=cycle>[,<delay in ms>", "Set voltage or current mode");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(HLWCFG, "HLWCFG", "<params,params,...>", "Configure sensor inputs", "Display configuration");

ATModeCommandHelpArrayPtr Sensor_HLW8012::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(HLWCAL),
        PROGMEM_AT_MODE_HELP_COMMAND(HLWMODE),
        PROGMEM_AT_MODE_HELP_COMMAND(HLWCFG),
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

static void print_sensor_input_settings(Stream &serial, Sensor_HLW8012::SensorInput &input, bool newLine)
{
    auto settings = input.getSettings();
    serial.printf_P(PSTR("%u,%u"), settings.intTime, settings.avgValCount);
    if (newLine) {
        serial.println();
    }
    else {
        serial.print(F(", "));
    }
};

bool Sensor_HLW8012::atModeHandler(AtModeArgs &args)
{
    if (Sensor_HLW80xx::atModeHandler(args)) {
        return true;
    }
    else {
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWCAL))) {
            auto &serial = args.getStream();
            if (args.requireArgs(1, 4)) {
                char ch = args.toLowerChar(0);
                if (args.size() >= 3) {
                    _dumpTimer.remove();
                    float value = (args.toFloat(2) * args.toFloat(3, 1.0f)) / args.toFloat(1);
                    auto &sensor = Plugins::Sensor::getWriteableConfig();
                    if (ch == 'u') {
                        _calibrationU = value;
                        sensor.hlw80xx.calibrationU = _calibrationU;
                        args.printf_P(PSTR("Voltage calibration set: %f"), value);
                    }
                    else if (ch == 'i') {
                        _calibrationI = value;
                        sensor.hlw80xx.calibrationI = _calibrationI;
                        args.printf_P(PSTR("Current calibration set: %f"), value);
                    }
                    else if (ch == 'p') {
                        _calibrationP = value;
                        sensor.hlw80xx.calibrationP = _calibrationP;
                        args.printf_P(PSTR("Power calibration set: %f"), value);
                    }
                    else {
                        args.print(F("Invalid setting"));
                    }
                }
                else {
                    _dumpTimer.remove();

                    struct {
                        float sum;
                        int count;
                        int max;
                    } data = { 0.0f, 0, args.toInt(1, 10) };

                    if (ch == 'u') {
                        args.print(F("Calibrating voltage"));
                        _calibrationU = 1;
                        _voltage = 0;
                        _setOutputMode(Sensor_HLW8012::OutputTypeEnum_t::VOLTAGE, 2000);
                        _Timer(_dumpTimer).add(500, true, [this, &serial, data](Event::CallbackTimerPtr timer) mutable {
                            if (_voltage) {
                                if (data.max-- == 0) {
                                    timer->disarm();
                                    _calibrationU = Plugins::Sensor::getConfig().hlw80xx.calibrationU;
                                }
                                data.sum += _voltage;
                                data.count++;
                                serial.printf_P(PSTR("+%s: % 10.6fV avg % 10.6fV\n"), PROGMEM_AT_MODE_HELP_COMMAND(HLWCAL), _voltage, data.sum / data.count);
                            }
                        });
                    }
                    else if (ch == 'i') {
                        args.print(F("Calibrating current"));
                        _calibrationI = 1;
                        float dimmingLevel = 0;
                        #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
                            dimmingLevel = _dimmingLevel;
                            _dimmingLevel = -1;
                        #endif
                        _current = 0;
                        _setOutputMode(Sensor_HLW8012::OutputTypeEnum_t::CURRENT, 2000);
                        _Timer(_dumpTimer).add(500, true, [this, &serial, data, dimmingLevel](Event::CallbackTimerPtr timer) mutable {
                            if (_current) {
                                if (data.max-- == 0) {
                                    timer->disarm();
                                    _calibrationI = Plugins::Sensor::getConfig().hlw80xx.calibrationI;
                                    #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
                                        _dimmingLevel = dimmingLevel;
                                    #endif
                                }
                                data.sum += _current;
                                data.count++;
                                serial.printf_P(PSTR("+%s: % 10.6fA avg % 10.6fA\n"), PROGMEM_AT_MODE_HELP_COMMAND(HLWCAL), _current, data.sum / data.count);
                            }
                        });
                    }
                    else if (ch == 'p') {
                        args.print(F("Calibrating power"));
                        _calibrationP = 1;
                        _power = 0;
                        _setOutputMode(Sensor_HLW8012::OutputTypeEnum_t::CYCLE, 2000);
                        _Timer(_dumpTimer).add(500, true, [this, &serial, data](Event::CallbackTimerPtr timer) mutable {
                            if (_power) {
                                if (data.max-- == 0) {
                                    timer->disarm();
                                    _calibrationP = Plugins::Sensor::getConfig().hlw80xx.calibrationP;
                                }
                                data.sum += _power;
                                data.count++;
                                serial.printf_P(PSTR("+%s: % 10.6fW avg % 10.6fW\n"), PROGMEM_AT_MODE_HELP_COMMAND(HLWCAL), _power, data.sum / data.count);
                            }
                        });
                    }
                    else {
                        args.print(F("Invalid setting"));
                    }
                }
            }
            return true;
        }
        if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWMODE))) {
            if (args.requireArgs(1, 2)) {
                auto type = F("Cycle Voltage/Current");
                Sensor_HLW8012::OutputTypeEnum_t mode = Sensor_HLW8012::OutputTypeEnum_t::CYCLE;

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

                args.printf_P(PSTR("Setting CF1 output mode to %s, delay %dms"), type, delay);
                _setOutputMode(mode, delay);
            }
            return true;
        }
        else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWCFG))) {
            if (args.isQueryMode()) {
                auto &serial = args.getStream();
                serial.printf_P(PSTR("+%s="), args.getCommand().c_str());
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
