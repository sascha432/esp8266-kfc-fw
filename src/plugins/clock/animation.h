/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_CLOCK
#define DEBUG_IOT_CLOCK                         1
#endif

#include <Arduino_compat.h>
#include "SevenSegmentPixel.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// number of digits
#ifndef IOT_CLOCK_NUM_DIGITS
#define IOT_CLOCK_NUM_DIGITS                    4
#endif

// pixels per segment
#ifndef IOT_CLOCK_NUM_PIXELS
#define IOT_CLOCK_NUM_PIXELS                    2
#endif

// number of colons
#ifndef IOT_CLOCK_NUM_COLONS
#if IOT_CLOCK_NUM_DIGITS == 4
#define IOT_CLOCK_NUM_COLONS                    1
#else
#define IOT_CLOCK_NUM_COLONS                    2
#endif
#endif

// pixels per dot
#ifndef IOT_CLOCK_NUM_COLON_PIXELS
#define IOT_CLOCK_NUM_COLON_PIXELS              2
#endif

// order of the segments (a-g)
#ifndef IOT_CLOCK_SEGMENT_ORDER
#define IOT_CLOCK_SEGMENT_ORDER                 { 0, 1, 3, 4, 5, 6, 2 }
#endif

// digit order, 30=colon #1,31=#2, etc...
#ifndef IOT_CLOCK_DIGIT_ORDER
#define IOT_CLOCK_DIGIT_ORDER                   { 0, 1, 30, 2, 3, 31, 4, 5 }
#endif

// pixel order of pixels that belong to digits, 0 to use pixel addresses of the 7 segment class
#ifndef IOT_CLOCK_PIXEL_ORDER
#define IOT_CLOCK_PIXEL_ORDER                   { 0 }
#endif

// update interval in ms, 0 to disable
#ifndef IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
#define IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL      25
#endif

// show rotating animation while time is invalid
#ifndef IOT_CLOCK_PIXEL_SYNC_ANIMATION
#define IOT_CLOCK_PIXEL_SYNC_ANIMATION               0
#endif

class ClockPlugin;

namespace Clock {

    using SevenSegmentDisplay = SevenSegmentPixel<uint8_t, IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_COLONS, IOT_CLOCK_NUM_COLON_PIXELS>;
    using PixelAddressType = SevenSegmentDisplay::pixel_address_t;
    using ColorType = SevenSegmentDisplay::color_t;
    using AnimationCallback = SevenSegmentDisplay::AnimationCallback_t;
    using LoopCallback = std::function<void(time_t now)>;

    enum class AnimationType : uint8_t {
        NONE = 0,
        RAINBOW,
        FLASHING,
        FADING,
        MAX
    };

    class Color {
    public:
        Color();
        Color(uint8_t *values);
        Color(uint8_t red, uint8_t green, uint8_t blue);
        Color (uint32_t value, bool bgr = false);

        static Color fromString(const String &value);
        static uint32_t get(uint8_t red, uint8_t green, uint8_t blue);

        String toString() const;
        String implode(char sep) const;

        Color &operator =(uint32_t value);
        operator int();

        uint32_t rnd();
        uint32_t get();

        uint8_t red() const;
        uint8_t green() const;
        uint8_t blue() const;
    private:
        uint8_t _red;
        uint8_t _green;
        uint8_t _blue;
    };

    class Animation {
    public:
        Animation(ClockPlugin &clock) : _clock(clock), _finished(false), _blinkColon(true) {
        }
        virtual ~Animation() {
            if (!_finished) {
                end();
            }
        }

        virtual void begin() {
            __LDBG_printf("begin");
            _finished = true;
        }
        virtual void end() {
            __LDBG_printf("end");
            _finished = true;
        }

        virtual void loop(time_t now);

        bool finished() const {
            return _finished;
        }
        bool doBlinkColon() const {
            return _blinkColon;
        }

    protected:
        LoopCallback _callback;
        ClockPlugin &_clock;
        uint8_t _finished : 1;
        uint8_t _blinkColon : 1;
    };

    class FadingAnimation : public Animation {
    public:
        static constexpr uint16_t kNoColorChange = ~0;
        static constexpr uint16_t kMaxDelay = kNoColorChange - 1;
        static constexpr float kFloatMaxProgress = std::numeric_limits<uint16_t>::max() - 1;
        static constexpr uint16_t kMaxProgress = kFloatMaxProgress;
        static constexpr uint16_t kUpdateRateHz = 50;
        static constexpr uint16_t kUpdateRate = 1000 / kUpdateRateHz;
        static constexpr float kProgressPerSecond = kFloatMaxProgress / kUpdateRateHz;
        static constexpr float kMaxSeconds = kProgressPerSecond;
        static constexpr float kMinSeconds = kProgressPerSecond / kFloatMaxProgress + 0.000001;

        static constexpr float kSpeedToSeconds(uint16_t speed) {
            return kProgressPerSecond / speed;
        }
        static constexpr uint16_t kSecondsToSpeed(float seconds) {
            return (kProgressPerSecond / seconds) + 0.5;
        }

    public:
        FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint16_t time = kNoColorChange);

        virtual void begin() override;

    private:
        void callback(time_t now);
        uint16_t _secondsToSpeed(float seconds) const;

    private:
        Color _from;
        Color _to;
        uint16_t _time;
        uint16_t _speed;
        uint16_t _progress;
    };

    class RainbowAnimation : public Animation {
    public:
        RainbowAnimation(ClockPlugin &clock, uint16_t speed, float multiplier, Color factor = Color(255, 255, 255));

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(time_t now) {}

    private:
        uint16_t _speed;
        Color _factor;
        float _multiplier;
        static constexpr uint8_t _mod = 120;
        static constexpr uint8_t _divMul = 40;
    };

    class FlashingAnimation : public Animation {
    public:
        FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(time_t now) {}

    private:
        Color _color;
        uint16_t _time;
    };

}
