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

// timeout in ms
#ifndef IOT_SENSOR_HLW8012_TIMEOUT
#define IOT_SENSOR_HLW8012_TIMEOUT              5000
#endif

// number of pulses required after switching
#ifndef IOT_SENSOR_HLW8012_CF1_MIN_COUNT
#define IOT_SENSOR_HLW8012_CF1_MIN_COUNT        1               // >= 1
#endif

// number of pulses before switching from voltage to current
#ifndef IOT_SENSOR_HLW8012_CF1_TOGGLE_COUNT
#define IOT_SENSOR_HLW8012_CF1_TOGGLE_COUNT     5               // >= IOT_SENSOR_HLW8012_CF1_MIN_COUNT
#endif


class Sensor_HLW8012 : public Sensor_HLW80xx {
public:
    typedef enum : uint8_t {
        CURRENT = 0,
        VOLTAGE = 1,
    } CF1OutputTypeEnum_t;

    typedef struct {
        uint32_t pulseWidth;
        double pulseWidthIntegral;
        unsigned long lastPulse;
        unsigned long timeout;
        uint32_t counter;
    } SensorInput_t;

    Sensor_HLW8012(const String &name, uint8_t pinSel, uint8_t pinCF, uint8_t pinCF1);
    virtual ~Sensor_HLW8012();

    void getStatus(PrintHtmlEntitiesString &output) override;
    virtual SensorEnumType_t getType() const override;
    virtual String _getId(const __FlashStringHelper *type = nullptr);

    static void loop();

private:

    void _callbackCF(unsigned long micros);
    void _callbackCF1(unsigned long micros);
    void _calcPulseWidth(SensorInput_t &input, unsigned long micros);

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
    SensorInput_t _input[2];
};

#endif
