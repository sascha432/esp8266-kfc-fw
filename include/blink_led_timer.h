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
#define BUILTIN_LED_STATEI(state)       (state)
#else
#define BUILTIN_LED_STATE(state)        state
#define BUILTIN_LED_STATEI(state)       (state ? LOW : HIGH)
#endif

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
#define BUILDIN_LED_SET(mode)           BlinkLEDTimer::setBlink(__LED_BUILTIN, mode);
#define forceTimeBUILDIN_LED_SETP(delay, patt)   BlinkLEDTimer::setPattern(__LED_BUILTIN, delay, patt);
#define BUILDIN_LED_GET(mode)           BlinkLEDTimer::isBlink(__LED_BUILTIN, mode)
#else
#define BUILDIN_LED_SET(...)
#define BUILDIN_LED_SETP(...)
#define BUILDIN_LED_GET(mode)           true
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
        INVALID = 0xffff
    };

    using Bitset = dynamic_bitset<56>;
    static constexpr size_t kBitsetSize = sizeof(Bitset);

    static const uint8_t INVALID_PIN = INVALID_PIN_ID;
    static const uint8_t IGNORE_BUILTIN_LED_PIN = IGNORE_BUILTIN_LED_PIN_ID;
    static const uint8_t NEOPIXEL_PIN = NEOPIXEL_PIN_ID;

    BlinkLEDTimer(uint8_t pin = INVALID_PIN);

    void set(uint16_t delay, Bitset &&pattern);
    void set(uint16_t delay, uint8_t pin, Bitset &&pattern);

    virtual void run() override;
    virtual void detach() override;

    // currently only one LED can blink at a time
    // if a different pin is used, the previously used pin is turned off
    static void setPattern(uint8_t pin, uint16_t delay, Bitset &&pattern);

    static void setBlink(uint8_t pin, uint16_t delay, int32_t color = -1);
    static void setBlink(uint8_t pin, BlinkType delay, int32_t color = -1);

    static bool isPattern(uint8_t pin, uint16_t delay, const Bitset &pattern);
    static bool isBlink(uint8_t pin, BlinkType delay);

    static bool isPinValid(uint8_t pin);

protected:
    static constexpr bool high() {
        return BUILTIN_LED_STATE(true);
    }

    static constexpr bool low() {
        return BUILTIN_LED_STATE(false);
    }

    bool state(bool state) const {
        return state ? high() : low();
    }

protected:
    uint8_t _pin;
    uint16_t _counter;
    Bitset _pattern;
    BlinkType _delay;
};

inline BlinkLEDTimer::BlinkLEDTimer(uint8_t pin) :
    _pin(pin),
    _delay(BlinkType::INVALID)
{
}

inline __attribute__((__always_inline__))
void BlinkLEDTimer::set(uint16_t delay, Bitset &&pattern)
{
    set(delay, _pin, std::move(pattern));
}

inline  __attribute__((__always_inline__))
void BlinkLEDTimer::setBlink(uint8_t pin, BlinkType delay, int32_t color)
{
    setBlink(pin, static_cast<uint16_t>(delay), color);
}

inline bool BlinkLEDTimer::isPinValid(uint8_t pin)
{
    return (pin != IGNORE_BUILTIN_LED_PIN) && (pin != NEOPIXEL_PIN) && (pin != INVALID_PIN);
}

extern BlinkLEDTimer *ledTimer;
