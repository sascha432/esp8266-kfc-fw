/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <OSTimer.h>
#include <dyn_bitset.h>
#include <stl_ext/memory.h>

#ifndef DEBUG_BLINK_LED_TIMER
#    define DEBUG_BLINK_LED_TIMER 0
#endif

#if DEBUG_BLINK_LED_TIMER
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

#define INVALID_PIN_ID            255
#define IGNORE_BUILTIN_LED_PIN_ID 255
#define NEOPIXEL_PIN_ID           253

#if __LED_BUILTIN == NEOPIXEL_PIN_ID
#    define BUILTIN_LED_NEOPIXEL 1
#else
#    define BUILTIN_LED_NEOPIXEL 0
#endif

#if INVERT_BUILTIN_LED
#    define BUILTIN_LED_STATE(state)  (state ? LOW : HIGH)
#    define BUILTIN_LED_STATE_INVERTED(state) (state)
#else
#    define BUILTIN_LED_STATE(state)  state
#    define BUILTIN_LED_STATE_INVERTED(state) (state ? LOW : HIGH)
#endif

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
#    define BUILTIN_LED_SET(mode)                  BlinkLEDTimer::setBlink(__LED_BUILTIN, mode);
#    define forceTimeBUILTIN_LED_SETP(delay, patt) BlinkLEDTimer::setPattern(__LED_BUILTIN, delay, patt);
#    define BUILTIN_LED_GET(mode)                  BlinkLEDTimer::isBlink(__LED_BUILTIN, mode)
#else
#    define BUILTIN_LED_SET(...)
#    define BUILTIN_LED_SETP(...)
#    define BUILTIN_LED_GET(mode) true
#endif

class BlinkLEDTimer : public OSTimer {
protected:
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
        NEOPIXEL_GREEN = SOLID,
        NEOPIXEL_YELLOW = FLICKER,
        NEOPIXEL_RED = SLOW,
        NEOPIXEL_BLUE = MEDIUM,
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

inline  __attribute__((__always_inline__))
bool BlinkLEDTimer::isPinValid(uint8_t pin)
{
    return (pin != IGNORE_BUILTIN_LED_PIN) && (pin != INVALID_PIN);
}

inline void BlinkLEDTimer::run()
{
    digitalWrite(_pin, BUILTIN_LED_STATE(_pattern.test(_counter++ % _pattern.size())));
}

inline void BlinkLEDTimer::detach()
{
    digitalWrite(_pin, low());
    OSTimer::detach();
}

extern BlinkLEDTimer *ledTimer;

#if BUILTIN_LED_NEOPIXEL

    #include <LoopFunctions.h>
    #include <reset_detector.h>

    #if HAVE_FASTLED
        #define FASTLED_INTERNAL
        #include <FastLED.h>
    #else
        #include <NeoPixelEx.h>
    #endif

    class WS2812LEDTimer : public BlinkLEDTimer {
    public:
        WS2812LEDTimer();

        // turn off or set a solid color
        void off();
        void solid(uint32_t color);

        //
        void set(uint32_t delay, uint8_t pin, Bitset &pattern);

        // color for blinking mode
        void setColor(uint32_t color);
        uint32_t getColor() const;
        void show();

        virtual void run() override;
        virtual void detach() override;

        static void init();
        static void terminate();

    private:
        uint32_t _color;

    public:
        #if HAVE_FASTLED
            static CRGB _pixels[__LED_BUILTIN_WS2812_NUM_LEDS];
        #else
            static NeoPixelEx::Strip<__LED_BUILTIN_WS2812_PIN, __LED_BUILTIN_WS2812_NUM_LEDS, NeoPixelEx::GRB, NeoPixelEx::TimingsWS2812> _pixels;
        #endif
    };

    inline void WS2812LEDTimer::init()
    {
        // add leds
        #if HAVE_FASTLED
            __LDBG_printf("init FastLED");
             FastLED.addLeds<NEOPIXEL, __LED_BUILTIN_WS2812_PIN>(_pixels, __LED_BUILTIN_WS2812_NUM_LEDS);
             #if HAVE_FASTLED_DITHER
                FastLED.setDither(BINARY_DITHER);
             #else
                FastLED.setDither(DISABLE_DITHER);
             #endif
            FastLED.clear(true);
        #else
            __LDBG_printf("init NeoPixelEx");
            _pixels.begin();
            _pixels.off();
        #endif
    }

    inline WS2812LEDTimer::WS2812LEDTimer() :
        BlinkLEDTimer(NEOPIXEL_PIN),
        _color(0)
    {
    }

    inline void WS2812LEDTimer::terminate()
    {
        stdex::reset(ledTimer);
        _pixels.off();
    }

    inline void WS2812LEDTimer::off()
    {
        #if HAVE_FASTLED
            FastLED.clear(true);
        #else
            _pixels.off();
        #endif
    }

    inline void WS2812LEDTimer::solid(uint32_t color)
    {
        #if HAVE_FASTLED
            fill_solid(_pixels, __LED_BUILTIN_WS2812_NUM_LEDS, CRGB(color));
        #else
            _pixels.fill(color);
        #endif
        show();
    }

    inline void WS2812LEDTimer::set(uint32_t delay, uint8_t pin, Bitset &pattern)
    {
        off();
        _pattern = pattern;
        startTimer(delay, true);
    }

    inline void WS2812LEDTimer::show()
    {
        #if HAVE_FASTLED
            FastLED.show();
        #else
            _pixels.show();
        #endif
    }

    inline void WS2812LEDTimer::setColor(uint32_t color)
    {
        _color = color;
    }

    inline uint32_t WS2812LEDTimer::getColor() const
    {
        return _color;
    }

    inline void WS2812LEDTimer::run()
    {
        if (!_pattern.test(_counter++ % _pattern.size())) {
            solid(0);
        }
        else {
            solid(_color);
        }
        show();
    }

    inline void WS2812LEDTimer::detach()
    {
        off();
        OSTimer::detach();
    }

#endif
