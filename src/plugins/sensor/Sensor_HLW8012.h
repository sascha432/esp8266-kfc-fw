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
#define IOT_SENSOR_HLW8012_TIMEOUT_I            2500
#endif

// timeout for power
#ifndef IOT_SENSOR_HLW8012_TIMEOUT_P
#define IOT_SENSOR_HLW8012_TIMEOUT_P            2500
#endif

// delay after switching before the sensor can be read
#ifndef IOT_SENSOR_HLW8012_DELAY_START
#define IOT_SENSOR_HLW8012_DELAY_START          750
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
    typedef enum : uint8_t {
        CURRENT = 1,
        VOLTAGE = 0,
    } CF1OutputTypeEnum_t;

    class SensorInput_t {
    public:
        SensorInput_t() : pulseWidth(0), pulseWidthIntegral(0), lastPulse(0), timeout(0), delayStart(0), toggleTimer(0), counter(0) {
        }

        uint32_t pulseWidth;
        double pulseWidthIntegral;
        unsigned long lastPulse;
        unsigned long timeout;
        unsigned long delayStart;
        unsigned long toggleTimer;
        uint32_t counter;
    } ;

    Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1);
    virtual ~Sensor_HLW8012();

    void getStatus(PrintHtmlEntitiesString &output) override;
    virtual SensorEnumType_t getType() const override;
    virtual String _getId(const __FlashStringHelper *type = nullptr);

    static void loop();

private:
    void _loop();

    void _callbackCF(unsigned long micros);
    void _callbackCF1(unsigned long micros);
    void _calcPulseWidth(SensorInput_t &input, unsigned long micros, uint16_t timeout);

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
    SensorInput_t _inputCF;
    SensorInput_t _inputCF1;
};

#endif
