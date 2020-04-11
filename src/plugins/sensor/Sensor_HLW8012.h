/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_HLW8012

#include <Arduino_compat.h>
#include <FixedCircularBuffer.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "Sensor_HLW80xx.h"

// pin configuration
#ifndef IOT_SENSOR_HLW8012_SEL
#define IOT_SENSOR_HLW8012_SEL                      16
#endif

#ifndef IOT_SENSOR_HLW8012_SEL_VOLTAGE
#define IOT_SENSOR_HLW8012_SEL_VOLTAGE              LOW
#endif

#ifndef IOT_SENSOR_HLW8012_SEL_CURRENT
#define IOT_SENSOR_HLW8012_SEL_CURRENT              HIGH
#endif

#ifndef IOT_SENSOR_HLW8012_CF
#define IOT_SENSOR_HLW8012_CF                       14
#endif

#ifndef IOT_SENSOR_HLW8012_CF1
#define IOT_SENSOR_HLW8012_CF1                      12
#endif

// delay after switching to voltage mode before the sensor can be read
#ifndef IOT_SENSOR_HLW8012_DELAY_START_U
#define IOT_SENSOR_HLW8012_DELAY_START_U            1000
#endif

// measure voltage duration
#ifndef IOT_SENSOR_HLW8012_MEASURE_LEN_U
#define IOT_SENSOR_HLW8012_MEASURE_LEN_U            1500
#endif

// delay after switching to current mode before the sensor can be read
#ifndef IOT_SENSOR_HLW8012_DELAY_START_I
#define IOT_SENSOR_HLW8012_DELAY_START_I            1250
#endif

// measure current duration
#ifndef IOT_SENSOR_HLW8012_MEASURE_LEN_I
#define IOT_SENSOR_HLW8012_MEASURE_LEN_I            12000
#endif

class Sensor_HLW8012 : public Sensor_HLW80xx {
public:
    typedef FixedCircularBuffer<uint32_t, 16> InterruptBuffer;
    typedef FixedCircularBuffer<uint32_t, 5> NoiseBuffer;

    typedef enum {
        CURRENT =           1,
        VOLTAGE =           0,
        POWER  =            2,
        CYCLE =             0xff,
    } OutputTypeEnum_t;

    typedef std::function<float(double pulseWidth)> ConvertCallback_t;

    class SensorInput {
    public:
        // sensor filter settings
        typedef struct {
            uint16_t intTime;           // timeframe for integration
            uint8_t avgValCount;        // use the last n items to create the average value for the integration
        } Settings_t;

        SensorInput(float &target) : _target(target) {
            clear();
            _settings.avgValCount = 2;
            _settings.intTime = 1000;
        }

        void clear() {
            pulseWidthIntegral = 0;
            lastIntegration = 0;
            delayStart = 0;
            toggleTimer = 0;
            counter = 0;
            average = 0;
        }

        double pulseWidthIntegral;
        uint32_t lastIntegration;
        uint32_t delayStart;
        uint32_t toggleTimer;
        uint32_t counter;
        uint32_t average;

    public:
        void setTarget(double pulseWidth) {
            if (pulseWidth == 0 || pulseWidth == NAN) {
                _target = NAN;
            }
            else {
                _target = _callback(pulseWidth);
            }
        }
        void setCallback(ConvertCallback_t callback) {
            _callback = callback;
        }

        float convertPulse(double pulseWidth) const {
            return _callback(pulseWidth);
        }

        float getTarget() const {
            return _target;
        }

        Settings_t &getSettings() {
            return _settings;
        }

    private:
        float &_target;
        ConvertCallback_t _callback;
        Settings_t _settings;
    };

    Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1);
    virtual ~Sensor_HLW8012();

    void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;
    virtual String _getId(const __FlashStringHelper *type = nullptr);

    static void loop();

    virtual void dump(Print &output) override;

    void _setOutputMode(OutputTypeEnum_t outputMode = CYCLE, int delay = -1);

private:
    friend Sensor_HLW80xx;

    void _loop();
    void _toggleOutputMode(int delay = -1);
    bool _processInterruptBuffer(InterruptBuffer &buffer, SensorInput &input);

    uint8_t _getCFPin() const {
        return _pinCF;
    }

    uint8_t _getCF1Pin() const {
        return _pinCF1;
    }

    uint8_t _pinSel;
    uint8_t _pinCF;
    uint8_t _pinCF1;
#if IOT_SENSOR_HLW80xx_NOISE_SUPPRESSION
    NoiseBuffer _noiseBuffer;
    float _noiseLevel;
#endif
    SensorInput _inputCF;
    SensorInput *_inputCF1;
    SensorInput _inputCFI;
    SensorInput _inputCFU;

    char _getOutputMode(SensorInput *input) const {
        if (input == &_inputCF) {
            return 'P';
        }
        else if (input == &_inputCFU) {
            return 'U';
        }
        else if (input == &_inputCFI) {
            return 'I';
        }
        return '?';
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

#endif
