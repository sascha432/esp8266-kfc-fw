/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <OSTimer.h>
#include <dyn_bitset.h>

#if INVERT_BUILTIN_LED
#define BUILTIN_LED_STATE(state)        (state ? LOW : HIGH)
#else
#define BUILTIN_LED_STATE(state)        state
#endif

class BlinkLEDTimer : public OSTimer {
public:
    typedef enum {
        SLOW = 1000,
        FAST = 250,
        FLICKER = 50,
        OFF = 0,
        SOLID = 1,
        SOS = 2,
    } BlinkDelayEnum_t;

    static const int8_t INVALID_PIN = -1;
    static const int8_t DEFAULT_PIN = -2;

    BlinkLEDTimer();

    virtual void run() override;
    void set(uint32_t delay, int8_t pin, dynamic_bitset &pattern);
    void detach();

    static void setPattern(int8_t pin, int delay, dynamic_bitset &pattern);
    static void setBlink(int8_t pin, uint16_t delay); // predefined values BlinkDelayEnum_t
    static void setBlink(uint16_t delay) {
        setBlink(DEFAULT_PIN, delay);
    }
    static void setBlink(BlinkDelayEnum_t delay) {
        setBlink((uint16_t)delay);
    }

private:
    int8_t _pin;
    uint16_t _counter;
    dynamic_bitset _pattern;
};

extern BlinkLEDTimer ledTimer;
