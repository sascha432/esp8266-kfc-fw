/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR && IOT_SENSOR_HAVE_HLW8012

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "Sensor_HLW80xx.h"

// pin configuration
#ifndef IOT_SENSOR_HLW8012_SEL
#define IOT_SENSOR_HLW8012_SEL                  16
#endif

#ifndef IOT_SENSOR_HLW8012_CF
#define IOT_SENSOR_HLW8012_CF                   14
#endif

#ifndef IOT_SENSOR_HLW8012_CF1
#define IOT_SENSOR_HLW8012_CF1                  12
#endif

// timeout for voltage in ms
#ifndef IOT_SENSOR_HLW8012_TIMEOUT_U
#define IOT_SENSOR_HLW8012_TIMEOUT_U            250
#endif

// timeout for current
#ifndef IOT_SENSOR_HLW8012_TIMEOUT_I
#define IOT_SENSOR_HLW8012_TIMEOUT_I            ((1000 * IOT_SENSOR_HLW80xx_TIMEOUT_MUL) + IOT_SENSOR_HLW8012_DELAY_START)
#endif

// timeout for power
#ifndef IOT_SENSOR_HLW8012_TIMEOUT_P
#define IOT_SENSOR_HLW8012_TIMEOUT_P            ((15000 * IOT_SENSOR_HLW80xx_TIMEOUT_MUL) + IOT_SENSOR_HLW8012_DELAY_START)
#endif

// delay after switching before the sensor can be read
#ifndef IOT_SENSOR_HLW8012_DELAY_START
#define IOT_SENSOR_HLW8012_DELAY_START          850
#endif

// time to measure voltage
#ifndef IOT_SENSOR_HLW8012_MEASURE_LEN_U
#define IOT_SENSOR_HLW8012_MEASURE_LEN_U        2000
#endif

// time to measure current
#ifndef IOT_SENSOR_HLW8012_MEASURE_LEN_I
#define IOT_SENSOR_HLW8012_MEASURE_LEN_I        10000
#endif

class Sensor_HLW8012 : public Sensor_HLW80xx {
public:
    typedef enum {
        CURRENT =           1,
        VOLTAGE =           0,
        CYCLE =             0xff
    } CF1OutputTypeEnum_t;

    class SensorInput {
    public:
        SensorInput() : pulseWidth(0), pulseWidthIntegral(0), lastPulse(0), timeout(0), delayStart(0), toggleTimer(0), counter(0) {
        }

        uint32_t pulseWidth;
        double pulseWidthIntegral;
        uint32_t lastPulse;
        uint32_t lastIntegration;
        uint32_t timeout;
        uint32_t delayStart;
        uint32_t toggleTimer;
        uint32_t counter;
    };

#if IOT_SENSOR_HLW8012_SENSOR_STATS
    class SensorStats {
    public:
        SensorStats() {
            type = '?';
            clear();
        }

        void clear() {
            sum = 0;
            sumIntegral = 0;
            count = 0;
            startTime = 0;
            endTime = 0;
            reset = 0;
        }

        void add(uint32_t width, uint32_t widthInt, uint16_t counter) {
            if (counter == 0) {
                reset++;
                sum = 0;
                sumIntegral = 0;
                count = 0;
            }
            else {
                sum += width;
                sumIntegral += widthInt;
                count++;
            }
        }

        void updateTime() {
            if (startTime == 0) {
                startTime = millis();
            }
            endTime = millis();
        }

        void dump(Print &output) {
            double avg = sum / (float)count;
            double avgI = sumIntegral / (float)count;
            double calc, calcI;
            double _calibrationU = 1.0;
            double _calibrationI = 1.0;
            double _calibrationP = 1.0;
            switch(type) {
                case 'I':
                    calc = IOT_SENSOR_HLW80xx_CALC_I(avg);
                    calcI = IOT_SENSOR_HLW80xx_CALC_I(avgI);
                    break;
                case 'P':
                    calc = IOT_SENSOR_HLW80xx_CALC_P(avg);
                    calcI = IOT_SENSOR_HLW80xx_CALC_P(avgI);
                    break;
                case 'U':
                default:
                    calc = IOT_SENSOR_HLW80xx_CALC_U(avg);
                    calcI = IOT_SENSOR_HLW80xx_CALC_U(avgI);
                    break;
            }
            output.printf_P("t=%c sum=%u sumI=%u avg=%.1f (%.4f) avgI=%.1f (%.4f) cnt=%u rst=%u time=%u",
                type, sum, sumIntegral,
                avg,
                calc,
                avgI,
                calcI,
                count, reset, endTime - startTime);
        }

        char type;
        uint32_t sum;
        uint32_t sumIntegral;
        uint16_t count;
        uint16_t reset;
        uint32_t startTime;
        uint32_t endTime;
    };

    SensorStats _stats[3];
#endif

    Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1);
    virtual ~Sensor_HLW8012();

    void getStatus(PrintHtmlEntitiesString &output) override;
    virtual SensorEnumType_t getType() const override;
    virtual String _getId(const __FlashStringHelper *type = nullptr);

    static void loop();

    virtual void dump(Print &output) override;

    void _setOutputMode(CF1OutputTypeEnum_t outputMode = CYCLE);

private:
    void _loop();

    void _callbackCF(unsigned long micros);
    void _callbackCF1(unsigned long micros);
    void _calcPulseWidth(SensorInput &input, unsigned long micros, uint16_t timeout);

    void _toggleOutputMode();

    uint8_t _getCFPin() const {
        return _pinCF;
    }

    uint8_t _getCF1Pin() const {
        return _pinCF1;
    }

    uint8_t _pinSel;
    uint8_t _pinCF;
    uint8_t _pinCF1;
    CF1OutputTypeEnum_t _output;
    SensorInput _inputCF;
    SensorInput _inputCF1;

#if IOT_SENSOR_HLW8012_RAW_DATA_DUMP
public:
    bool _startRawDataDump(const String &filename, uint32_t endTime);
    void _simulateCallbackCF1(uint32_t interval);
    static Sensor_HLW8012 *_getFirstSensor();
    void _endRawDataDump();

private:
    File _rawDumpFile;
    uint32_t _rawDumpEndTime;
#endif
};

#endif
