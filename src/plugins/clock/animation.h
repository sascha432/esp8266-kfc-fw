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
#define IOT_CLOCK_NUM_DIGITS                        4
#endif

// pixels per segment
#ifndef IOT_CLOCK_NUM_PIXELS
#define IOT_CLOCK_NUM_PIXELS                        2
#endif

// number of colons
#ifndef IOT_CLOCK_NUM_COLONS
#if IOT_CLOCK_NUM_DIGITS == 4
#define IOT_CLOCK_NUM_COLONS                        1
#else
#define IOT_CLOCK_NUM_COLONS                        2
#endif
#endif

// pixels per dot
#ifndef IOT_CLOCK_NUM_COLON_PIXELS
#define IOT_CLOCK_NUM_COLON_PIXELS                  2
#endif

// order of the segments (a-g)
#ifndef IOT_CLOCK_SEGMENT_ORDER
#define IOT_CLOCK_SEGMENT_ORDER                     { 0, 1, 3, 4, 5, 6, 2 }
#endif

// digit order, 30=colon #1,31=#2, etc...
#ifndef IOT_CLOCK_DIGIT_ORDER
#define IOT_CLOCK_DIGIT_ORDER                       { 0, 1, 30, 2, 3, 31, 4, 5 }
#endif

// pixel order of pixels that belong to digits, 0 to use pixel addresses of the 7 segment class
#ifndef IOT_CLOCK_PIXEL_ORDER
#define IOT_CLOCK_PIXEL_ORDER                       { 0 }
#endif

// update interval in ms, 0 to disable
#ifndef IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
#define IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL          125
#endif

// show rotating animation while time is invalid
#ifndef IOT_CLOCK_PIXEL_SYNC_ANIMATION
#define IOT_CLOCK_PIXEL_SYNC_ANIMATION              0
#endif

class ClockPlugin;

namespace Clock {

    using SevenSegmentDisplay = SevenSegmentPixel<uint8_t, IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_COLONS, IOT_CLOCK_NUM_COLON_PIXELS>;
    using PixelAddressType = SevenSegmentDisplay::PixelAddressType;
    using ColorType = SevenSegmentDisplay::ColorType;
    using AnimationCallback = SevenSegmentDisplay::AnimationCallback;
    using LoopCallback = std::function<void(time_t now)>;

    static constexpr auto kTotalPixelCount = SevenSegmentDisplay::kTotalPixelCount;
    static constexpr uint16_t kMaxBrightness = SevenSegmentDisplay::kMaxBrightness;
    static constexpr uint16_t kBrightness75 = kMaxBrightness * 0.75;
    static constexpr uint16_t kBrightness50 = kMaxBrightness * 0.5;
    static constexpr uint16_t kBrightnessTempProtection = kMaxBrightness * 0.25;

    enum class AnimationType : uint8_t {
        MIN = 0,
        NONE = 0,
        RAINBOW,
        FLASHING,
        FADING,
        MAX,
        NEXT,
    };

    class Color {
    public:
        // possible pairs
        // (4, 85), (6, 51), (16, 17), (18, 15), (52, 5), (86, 3)
        static constexpr uint8_t kRndMod = 6;
        static constexpr uint8_t kRndMul = 51;
        static constexpr uint8_t kRndAnyAbove = 127;
        static_assert((kRndMod - 1) * kRndMul == 255, "invalid mod or mul");

    public:
        Color();
        Color(uint8_t *values);
        Color(uint8_t red, uint8_t green, uint8_t blue);
        Color(uint32_t value);

        static Color fromString(const String &value);
        static Color fromBGR(uint32_t value);

        String toString() const;
        String implode(char sep) const;

        Color &operator=(uint32_t value);
        operator bool() const;
        operator int() const;
        operator uint32_t() const;
        bool operator==(int value) const;
        bool operator!=(int value) const;
        bool operator==(uint32_t value) const;
        bool operator!=(uint32_t value) const;

        // set random color
        uint32_t rnd(uint8_t minValue = kRndAnyAbove);
        // uint32_t rnd(Color factor, uint8_t minValue = kRndAnyAbove);

        uint32_t get() const;
        uint8_t red() const;
        uint8_t green() const;
        uint8_t blue() const;

    private:
        uint8_t _getRand(uint8_t mod, uint8_t mul, uint8_t factor = 255);

        union __attribute__packed__ {
            struct __attribute__packed__ {
                uint8_t _blue;
                uint8_t _green;
                uint8_t _red;
            };
            uint32_t _value: 24;
        };

    };

    static_assert(sizeof(Color) == 3, "Invalid size");

    class Animation {
    public:
        static constexpr uint16_t kDefaultUpdateRate = 100;

    public:
        Animation(ClockPlugin &clock);
        virtual ~Animation();

        // call to start and end the animation. begin might be called after end again...
        virtual void begin();
        virtual void end();

        // loop function
        // can be overriden otherwise it calls _callback @ _updateRate milliseconds
        virtual void loop(time_t now);

        // animation has finsihed
        // can be restarted with begin
        bool finished() const;
        // does the animation want the colons to blink, if that is enabled
        bool doBlinkColon() const;
        // get update rate set for this animation
        uint16_t getUpdateRate() const;

    protected:
        // set update rate for animation and clock
        void setUpdateRate(uint16_t updateRate);
        // get update rate for the clock
        uint16_t getClockUpdateRate() const;

        // set current color
        void setColor(Color color);
        // get current color
        Color getColor() const;

        // set pixel animation callback
        //
        // this callback is used during the refresh of the display, and the callback is invoked for
        // every single pixel. the pixel address, color and milliseconds are passed as arguments and the
        // return value is the color that will be set. make sure that calling this function kTotalPixelCount
        // times fits into the update rate interval. millis is usally set once before the rendering starts
        //
        // _display.setMillis(millis());
        // _display.setDigit(0, 8, color);
        // _display.setDigit(1, 8, color);
        // ...
        // _display.show();
        //
        // std::function<ColorType(PixelAddressType address, ColorType color, uint32_t millis)>
        void setAnimationCallback(AnimationCallback callback);
        void removeAnimationCallback();

        // set loop callback
        // it is invoked at the current update rate
        void setLoopCallback(LoopCallback callback);
        void removeLoopCallback();

    private:
        ClockPlugin &_clock;
    protected:
        LoopCallback _callback;
        uint16_t _updateRate;
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
        static constexpr float kMinSeconds = kProgressPerSecond / kFloatMaxProgress + 0.00001;

        static constexpr float kSpeedToSeconds(uint16_t speed) {
            return kProgressPerSecond / speed;
        }
        static constexpr uint16_t kSecondsToSpeed(float seconds) {
            return (kProgressPerSecond / seconds) + 0.5;
        }

    public:
        FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint16_t time = kNoColorChange, Color factor = 0xffffffU);

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
        Color _factor;
    };

    class RainbowAnimation : public Animation {
    public:
        RainbowAnimation(ClockPlugin &clock, uint16_t speed, float multiplier, Color factor = 0xffffffU, Color minimum = 0U);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(time_t now) {}

    private:
        Color _color(uint8_t red, uint8_t green, uint8_t blue) const;

        uint16_t _speed;
        Color _factor;
        Color _minimum;
        float _multiplier;
        static constexpr uint8_t _mod = 120;
        static constexpr uint8_t _divMul = 40;
    };

    class FlashingAnimation : public Animation {
    public:
        FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time, uint8_t mod = 2);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(time_t now) {}

    private:
        Color _color;
        uint16_t _time;
        uint8_t _mod;
    };

    class CallbackAnimation : public Animation {
    public:
        CallbackAnimation(ClockPlugin &clock, AnimationCallback callback, uint16_t updateRate = 20, bool blinkColon = true);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(time_t now) {}

    private:
        AnimationCallback _callback;
        uint16_t _updateRate;
        bool _blinkColon;
    };

}
