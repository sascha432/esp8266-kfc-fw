/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_CLOCK
#define DEBUG_IOT_CLOCK                         1
#endif

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "SevenSegmentPixel.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_LED_MATRIX

#define IOT_CLOCK_NUM_PIXELS                        (IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS)

// first pixel to use, others can be controlled separately and are reset during reboot only
#ifndef IOT_LED_MATRIX_START_ADDR
#define IOT_LED_MATRIX_START_ADDR                   0
#endif

#else


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

#endif

class ClockPlugin;

using KFCConfigurationClasses::Plugins;

namespace Clock {

#if IOT_LED_MATRIX
    using SevenSegmentDisplay = SevenSegmentPixel<uint16_t, 1, IOT_CLOCK_NUM_PIXELS, 0, 0>;
#else
    using SevenSegmentDisplay = SevenSegmentPixel<uint8_t, IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_COLONS, IOT_CLOCK_NUM_COLON_PIXELS>;
#endif

    using Config_t = Plugins::Clock::ClockConfig_t;
    using PixelAddressType = SevenSegmentDisplay::PixelAddressType;
    using ColorType = SevenSegmentDisplay::ColorType;
    using ClockColor_t = Plugins::Clock::ClockColor_t;
    using AnimationCallback = SevenSegmentDisplay::AnimationCallback;
    using LoopCallback = std::function<void(uint32_t millisValue)>;
    using AnimationType = Config_t::AnimationType;
    using InitialStateType = Config_t::InitialStateType;
#if IOT_LED_MATRIX
#if IOT_LED_MATRIX_ROWS > 255 || IOT_LED_MATRIX_COLS > 255
    using CoordinateType = uint16_t;
#else
    using CoordinateType = uint8_t;
#endif
#else
    using CoordinateType = PixelAddressType;
#endif

    static constexpr auto kTotalPixelCount = SevenSegmentDisplay::kTotalPixelCount;
    static constexpr uint8_t kMaxBrightness = SevenSegmentDisplay::kMaxBrightness;
    static constexpr uint8_t kBrightness75 = kMaxBrightness * 0.75;
    static constexpr uint8_t kBrightness50 = kMaxBrightness * 0.5;
    static constexpr uint8_t kBrightnessTempProtection = kMaxBrightness * 0.25;

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
        Color &operator=(ClockColor_t value);
        operator bool() const;
        operator int() const;
        operator uint32_t() const;
        operator ClockColor_t() const;
        bool operator==(int value) const;
        bool operator!=(int value) const;
        bool operator==(uint32_t value) const;
        bool operator!=(uint32_t value) const;
        bool operator==(const Color &value) const;
        bool operator!=(const Color &value) const;

        // set random color
        uint32_t rnd(uint8_t minValue = kRndAnyAbove);
        // uint32_t rnd(Color factor, uint8_t minValue = kRndAnyAbove);

        uint32_t get() const;
        uint8_t red() const;
        uint8_t green() const;
        uint8_t blue() const;

        uint8_t &red();
        uint8_t &green();
        uint8_t &blue();

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

    inline bool Color::operator==(uint32_t value) const
    {
        return _value == value;
    }

    inline bool Color::operator!=(uint32_t value) const
    {
        return _value != value;
    }

    inline bool Color::operator==(int value) const
    {
        return _value == value;
    }

    inline bool Color::operator!=(int value) const
    {
        return _value != value;
    }

    inline bool Color::operator==(const Color &value) const
    {
        return value.get() == get();
    }

    inline bool Color::operator!=(const Color &value) const
    {
        return value.get() != get();
    }

    inline uint32_t Color::get() const
    {
        return _value;
    }

    inline Color &Color::operator=(uint32_t value)
    {
        _value = value;
        return *this;
    }

    inline Color &Color::operator=(ClockColor_t value)
    {
        _value = value.value;
        return *this;
    }

    inline Color::operator bool() const
    {
        return _value != 0;
    }

    inline Color::operator int() const
    {
        return _value;
    }

    inline Color::operator uint32_t() const
    {
        return _value;
    }

    inline Color::operator ClockColor_t() const
    {
        return ClockColor_t(_value);
    }

    inline uint8_t Color::red() const
    {
        return _red;
    }

    inline uint8_t Color::green() const
    {
        return _green;
    }

    inline uint8_t Color::blue() const
    {
        return _blue;
    }

    inline uint8_t &Color::red()
    {
        return _red;
    }

    inline uint8_t &Color::green()
    {
        return _green;
    }

    inline uint8_t &Color::blue()
    {
        return _blue;
    }


    static_assert(sizeof(Color) == 3, "Invalid size");

    // ------------------------------------------------------------------------
    // Animation Base Class

    class Animation {
    public:
        static constexpr uint16_t kDefaultUpdateRate = 100;

        class Point {
        public:
            Point(CoordinateType row = 0, CoordinateType col = 0) : _row(row), _col(col) {}

            void setPoint(CoordinateType row, CoordinateType col) {
                _row = row;
                _col = col;
            }

            CoordinateType getRow() const {
                return _row;
            }

            CoordinateType getInvertedRow() const {
                return IOT_LED_MATRIX_ROWS - _row;
            }

            void setRow(CoordinateType row) {
                _row = row;
            }

            CoordinateType getCol() const {
                return _col;
            }

            CoordinateType getInvertedCol() const {
                return IOT_LED_MATRIX_COLS - _col;
            }

            void setCol(CoordinateType col) {
                _col = col;
            }

            void rotate90() {
                std::swap(_row, _col);
            }

        private:
            CoordinateType _row;
            CoordinateType _col;
        };

    public:
        Animation(ClockPlugin &clock);
        Animation(ClockPlugin &clock, uint16_t rows, uint16_t cols);
        virtual ~Animation();

        // call to start and end the animation. begin might be called after end again...
        virtual void begin();
        virtual void end();

        // loop function
        // can be overriden otherwise it calls _callback @ _updateRate milliseconds
        virtual void loop(uint32_t millisValue);

        // animation has finsihed
        // can be restarted with begin
        bool finished() const;
        // does the animation want the colons to blink, if that is enabled
        bool doBlinkColon() const;
        // get update rate set for this animation
        uint16_t getUpdateRate() const;

        static constexpr uint16_t rows() {
            return IOT_LED_MATRIX_ROWS;
        }
        static constexpr uint16_t cols() {
            return IOT_LED_MATRIX_COLS;
        }

        Point dimensions() const {
            return Point(rows(), cols());
        }

        Point translateAddress(PixelAddressType address) const {
#if IOT_LED_MATRIX_ROWS == 1
            auto p = Point(0, address);
#elif IOT_LED_MATRIX_COLS == 1
            auto p = Point(address, 0);
#else
            auto p = Point(address % rows(), address / rows());
            if (p.getCol() % 2 != 0) {
                p.setRow((rows() - 1) - p.getRow());
            }
#endif
            return p;
        }

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
        // times fits into the update rate interval. millis is set once before the rendering starts
        // the same value for millis must produce the same result. srand() is set to millis before rendering
        // a new frame
        //
        // _display.setMillis(millis());
        // _display.setDigit(0, 8, color);
        // _display.setDigit(1, 8, color);
        // ...
        // _display.show();
        //
        // std::function<ColorType(PixelAddressType address, ColorType color, uint32_t millisValue)>
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

    // ------------------------------------------------------------------------
    // FadingAnimation

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
        void callback(uint32_t millisValue);
        uint16_t _secondsToSpeed(float seconds) const;

    private:
        Color _from;
        Color _to;
        uint16_t _time;
        uint16_t _speed;
        uint16_t _progress;
        Color _factor;
    };

    // ------------------------------------------------------------------------
    // RainbowAnimation

    class RainbowAnimation : public Animation {
    public:
        using RainbowMultiplier = Plugins::ClockConfig::RainbowMultiplier_t;
        using RainbowColor = Plugins::ClockConfig::RainbowColor_t;
    public:
        RainbowAnimation(ClockPlugin &clock, uint16_t speed, RainbowMultiplier &multiplier, RainbowColor &color);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(uint32_t millisValue);

    private:
        Color _normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const;
        void _increment(float *valuePtr, float min, float max, float *incrPtr);

        uint16_t _speed;
        RainbowMultiplier &_multiplier;
        RainbowColor &_color;
        struct ColorFactor {
            float _red;
            float _green;
            float _blue;
            ColorFactor(uint32 value) : _red(value & 0xff), _green((value >> 8) & 0xff), _blue((value >> 16) & 0xff) {}
            float red() const {
                return _red;
            }
            float green() const {
                return _green;
            }
            float blue() const {
                return _blue;
            }
        } _factor;
        uint32_t _lastUpdate;

        static constexpr uint8_t _mod = 120;
        static constexpr uint8_t _divMul = 40;
    };

    // ------------------------------------------------------------------------
    // FlashingAnimation

    class FlashingAnimation : public Animation {
    public:
        FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time, uint8_t mod = 2);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(uint32_t millisValue) {}

    private:
        Color _color;
        uint16_t _time;
        uint8_t _mod;
    };

    // ------------------------------------------------------------------------
    // SkipRowsAnimation
    // TODO finish SkipRowsAnimation

    class SkipRowsAnimation : public Animation {
    public:
        SkipRowsAnimation(ClockPlugin &clock, uint16_t row, uint16_t col, uint32_t time);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(uint32_t millisValue) {}

    private:
        uint32_t _time;
        uint16_t _row;
        uint16_t _col;
    };

    // ------------------------------------------------------------------------
    // FireAnimation
    // TODO finish FireAnimation

    class FireAnimation : public Animation {
    public:
        // adapted from an example in FastLED, which is adapted from work done by Mark Kriegsman (called Fire2012).
        using FireAnimationConfig = Plugins::ClockConfig::FireAnimation_t;
        using Orientation = Plugins::ClockConfig::FireAnimation_t::Orientation;

    private:
        class Line {
        public:

            Line(const Line &) = delete;
            Line(Line &&move) : _num(std::exchange(move._num, 0)), _heat(std::exchange(move._heat, nullptr)) {}

            Line &operator==(const Line &) = delete;
            Line &operator==(Line &&move) {
                if (_heat) {
                    delete[] _heat;
                }
                _heat = std::exchange(move._heat, nullptr);
                _num = std::exchange(move._num, 0);
                return *this;
            }

            Line() : _num(0), _heat(nullptr) {}
            Line(uint16_t num) : _num(num), _heat(new uint8_t[_num]()) {
                if (!_heat) {
                    _num = 0;
                }
            }
            ~Line() {
                if (_heat) {
                    delete[] _heat;
                }
            }

            void init(uint16_t num) {
                this->~Line();
                _num = num;
                _heat = new uint8_t[_num]();
                if (!_heat) {
                    _num = 0;
                }
            }

            void cooldown(uint8_t cooling) {
                // Step 1.  Cool down every cell a little
                for (uint16_t i = 0; i < _num; i++) {
                    uint8_t cooldownValue = rand() % (((cooling * 10) / _num) + 2);
                    if (cooldownValue > _heat[i]) {
                        _heat[i] = 0;
                    }
                    else {
                        _heat[i] -= cooldownValue;
                    }
                }
            }

            void heatup()  {
                // Step 2.  Heat from each cell drifts 'up' and diffuses a little
                for(uint16_t k = _num - 1; k >= 2; k--) {
                    _heat[k] = (_heat[k - 1] + _heat[k - 2] + _heat[k - 2]) / 3;
                }
            }

            void ignite(uint8_t sparking) {
                // Step 3.  Randomly ignite new 'sparks' near the bottom
                auto n = std::max<uint8_t>(_num / 5, 2);
                if (random(255) < sparking) {
                    uint8_t y = rand() % n;
                    _heat[y] += (rand() % (255 - 160)) + 160;
                    //heat[y] = random(160, 255);
                }
            }

            uint16_t getNum() const {
                return _num;
            }

            Color getHeatColor(uint16_t num) {
                if (num >= _num) {
                    return 0U;
                }
                // Step 4.  Convert heat to LED colors

                // Scale 'heat' down from 0-255 to 0-191
                uint8_t t192 = round((_heat[num] / 255.0) * 191);

                // calculate ramp up from
                uint8_t heatramp = t192 & 0x3F; // 0..63
                heatramp <<= 2; // scale up to 0..252

                // figure out which third of the spectrum we're in:
                if (t192 > 0x80) {                     // hottest
                    return Color(255, 255, heatramp);
                }
                else if (t192 > 0x40) {             // middle
                    return Color(255, heatramp, 0);
                }
                return Color(heatramp, 0, 0);
            }

        private:
            uint16_t _num;
            uint8_t *_heat;
        };

    public:
        FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg);
        ~FireAnimation();

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(uint32_t millisValue) {
            // update all lines
            srand(millisValue);
            for(uint16_t i = 0; i < _lineCount; i++) {
                _lines[i].cooldown(_cfg.cooling);
                _lines[i].heatup();
                _lines[i].ignite(_cfg.sparking);
            }
        }

    private:
        uint16_t _lineCount;
        Line *_lines;
        FireAnimationConfig &_cfg;
    };

    // ------------------------------------------------------------------------
    // Callback

    class CallbackAnimation : public Animation {
    public:
        CallbackAnimation(ClockPlugin &clock, AnimationCallback callback, uint16_t updateRate = 20, bool blinkColon = true);

        virtual void begin() override;
        virtual void end() override;
        virtual void loop(uint32_t millisValue) {}

    private:
        AnimationCallback _callback;
        uint16_t _updateRate;
        bool _blinkColon;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
