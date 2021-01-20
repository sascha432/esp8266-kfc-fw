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

#if __LED_BUILTIN != -1
#define BUILDIN_LED_SET(mode)        BlinkLEDTimer::setBlink(__LED_BUILTIN, mode);
#else
#define BUILDIN_LED_SET(mode)
#endif


class BlinkLEDTimer : public OSTimer {
public:
    enum class BlinkType : uint16_t {
        SLOW = 1000,
        MEDIUM = 500,
        FAST = 250,
        FLICKER = 50,
        OFF = 0,
        SOLID = 1,
        SOS = 2,
    };

    static const int8_t INVALID_PIN = -1;

    BlinkLEDTimer();

    virtual void run() override;
    void set(uint32_t delay, int8_t pin, dynamic_bitset &&pattern);
    virtual void detach() override;

    static void setPattern(int8_t pin, int delay, dynamic_bitset &&pattern);
    //static void setPattern(int8_t pin, int delay, const dynamic_bitset &pattern);
    static void setBlink(int8_t pin, BlinkType delay, int32_t color = -1) {
        setBlink(pin, static_cast<uint16_t>(delay), color);
    }
    static void setBlink(int8_t pin, uint16_t delay, int32_t color = -1); // predefined values BlinkDelayEnum_t
    // static bool isOn();

protected:
    int8_t _pin;
    uint16_t _counter;
    dynamic_bitset _pattern;
    // bool _on;
};

extern BlinkLEDTimer *ledTimer;
