/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <OSTimer.h>
#include <dyn_bitset.h>

#define INVALID_PIN_ID                  255
#define IGNORE_BUILTIN_LED_PIN_ID       255
#define NEOPIXEL_PIN_ID                 253

#if INVERT_BUILTIN_LED
#define BUILTIN_LED_STATE(state)        (state ? LOW : HIGH)
#else
#define BUILTIN_LED_STATE(state)        state
#endif

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
#define BUILDIN_LED_SET(mode)        BlinkLEDTimer::setBlink(__LED_BUILTIN, mode);
#else
#define BUILDIN_LED_SET(mode)
#endif


class BlinkLEDTimer : public OSTimer {
private:
    using OSTimer::startTimer;
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

    static const uint8_t INVALID_PIN = INVALID_PIN_ID;
    static const uint8_t IGNORE_BUILTIN_LED_PIN = IGNORE_BUILTIN_LED_PIN_ID;
    static const uint8_t NEOPIXEL_PIN = NEOPIXEL_PIN_ID;

    BlinkLEDTimer(uint8_t pin = INVALID_PIN) : _pin(pin) {}

    void set(uint32_t delay, dynamic_bitset &&pattern);

    inline __attribute__((__always_inline__))
    void set(uint32_t delay, uint8_t pin, dynamic_bitset &&pattern) {
        _pin = pin;
        set(delay, std::move(pattern));
    }

    virtual void run() override;
    virtual void detach() override;

    static void setPattern(uint8_t pin, int delay, dynamic_bitset &&pattern);
    //static void setPattern(int8_t pin, int delay, const dynamic_bitset &pattern);
    static void setBlink(uint8_t pin, BlinkType delay, int32_t color = -1) {
        setBlink(pin, static_cast<uint16_t>(delay), color);
    }
    static void setBlink(uint8_t pin, uint16_t delay, int32_t color = -1); // predefined values BlinkDelayEnum_t
    // static bool isOn();

    static bool isPinValid(uint8_t pin) {;
        return pin != IGNORE_BUILTIN_LED_PIN && pin != NEOPIXEL_PIN;
    }

protected:
    uint8_t _pin;
    uint16_t _counter;
    dynamic_bitset _pattern;
    // bool _on;
};

extern BlinkLEDTimer *ledTimer;
