/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "color.h"
#include "pixel_display.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace Clock {

    using DisplayBufferType = Clock::PixelDisplayBuffer<0, IOT_LED_MATRIX_ROWS, IOT_LED_MATRIX_COLS, true, false, true, true>;

#if IOT_LED_MATRIX

    using DisplayType = Clock::PixelDisplay<
            Clock::NeoPixelController<IOT_CLOCK_LED_PIN>,
            Clock::PixelDisplayBuffer<IOT_LED_MATRIX_START_ADDR, IOT_LED_MATRIX_ROWS, IOT_LED_MATRIX_COLS, true, false, true, true>
        >;

#else

#error TODO

#endif

    using CoordinateType = DisplayType::CoordinateType;
    using PixelAddressType = DisplayType::PixelAddressType;
    using PixelCoordinatesType = DisplayType::PixelCoordinatesType;

    // ------------------------------------------------------------------------
    // Animation Base Class
    //
    // method to override
    //
    // begin
    //
    // gets called once the object is assigned to the clock
    //
    // loop
    //
    // gets called every loop, ~1ms
    //
    // copyTo
    //
    // copy buffered frame to display or create frame inside display directly
    // copies buffer into display by default
    //
    // blend
    //
    // blend two animations into a single frame and copy it to the display
    //
    // nextFrame
    //
    // create next frame in buffer
    // gets called before blend and copyTo
    //
    //
    //


    class Animation {
    public:
        static constexpr uint16_t kDefaultBlendTime = 4000;
        static constexpr bool kDefaultDisableBlinkColon = true;
        static constexpr CoordinateType kRows = IOT_LED_MATRIX_ROWS;
        static constexpr CoordinateType kCols = IOT_LED_MATRIX_COLS;
        static constexpr PixelAddressType kNumPixels = kRows * kCols;

        enum class ModeType {
            NONE,
            ACTIVE,
            BLEND_IN,
            BLEND_OUT,
        };

    public:
        Animation(ClockPlugin &clock, Color color = 0U) :
            _parent(clock),
            _display(nullptr),
            _buffer(nullptr),
            _blendTime(kDefaultBlendTime),
            _disableBlinkColon(kDefaultDisableBlinkColon),
            _mode(ModeType::NONE),
            _color(color)
        {
        }

        // Animation(ClockPlugin &clock, DisplayBufferType *buffer) :
        //     _parent(clock),
        //     _display(nullptr),
        //     _buffer(buffer),
        //     _blendTime(kDefaultBlendTime),
        //     _disableBlinkColon(kDefaultDisableBlinkColon)
        // {
        // }

        // Animation(ClockPlugin &clock, DisplayType &display) :
        //     _parent(clock),
        //     _display(&display),
        //     _buffer(nullptr),
        //     _blendTime(kDefaultBlendTime),
        //     _disableBlinkColon(kDefaultDisableBlinkColon)
        // {
        // }

        virtual ~Animation()
        {
            _freeBuffer();
        }

        virtual void begin()
        {
        }

        virtual bool blend(Animation *animation, DisplayType &display, uint16_t timeLeft, uint32_t millisValue)
        {
            if (timeLeft >= _blendTime) {
                _freeBuffer();
                return false;
            }
            // __LDBG_printf("blend buf=%p ani=%p display=%p time=%u/%u", _buffer, animation, &display, timeLeft, _blendTime);
            if (!hasBuffer()) {
                _buffer = new DisplayBufferType();
                if (_buffer) {
                    copyTo(*_buffer, millisValue);
                }
            }
            if (animation) {
                if (!animation->blend(nullptr, display, timeLeft, millisValue)) {
                    _freeBuffer();
                    return false;
                }
                fract8 amount = ((timeLeft * 255) / _blendTime);
                ::blend(animation->getBuffer()->begin(), _buffer->begin(), display.begin(), display.kNumPixels, amount);
            }
            return true;
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue)
        {
            __LDBG_printf("_buffer=%p display=%u", _buffer, &display);
            if (hasBuffer()) {
                display.copy(*_buffer, display);
            }
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue)
        {
            __LDBG_printf("_buffer=%p buffer=%u", _buffer, &buffer);
            if (hasBuffer()) {
                buffer.copy(*_buffer, buffer);
            }
        }

        virtual void loop(uint32_t millisValue)
        {
        }

        virtual void nextFrame(uint32_t millis)
        {
        }

        bool hasDisplay() const
        {
            return _display != nullptr;
        }

        bool hasBuffer() const
        {
            return _buffer != nullptr;
        }

        void setDisplay(DisplayType *display) {
            _freeBuffer();
            _display = display;
        }

        void setBuffer(DisplayBufferType *buffer) {
            _freeBuffer(buffer);
            _display = nullptr;
        }

        DisplayBufferType *getBuffer() {
            return _buffer;
        }

        void setBlendTime(uint16_t blendTime)
        {
            __LDBG_printf("blend_time=%u", blendTime);
            _blendTime = blendTime;
        }

        bool isBlinkColonDisabled() const
        {
            return _disableBlinkColon;
        }

        virtual void setColor(Color color)
        {
            _color = color;
        }

        void setMode(ModeType mode)
        {
            _mode = mode;
        }

    protected:
        Color _getColor() const;
        void _setColor(Color color);

        void _freeBuffer(DisplayBufferType *newBuffer = nullptr) {
            if (_buffer) {
                __LDBG_printf("delete _buffer=%p", _buffer);
                delete _buffer;
            }
            _buffer = newBuffer;
        }

        ClockPlugin &_parent;
        DisplayType *_display;
        DisplayBufferType *_buffer;

    protected:
        uint16_t _blendTime;
        bool _disableBlinkColon;
        ModeType _mode;
        Color _color;
    };

    // ------------------------------------------------------------------------
    // FadingAnimation
    class FadingAnimation : public Animation {
    public:
        static constexpr uint32_t kNoColorChange = ~0;
        static constexpr uint16_t kMaxDelay = static_cast<uint16_t>(~0) - 1;
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
        FadingAnimation(ClockPlugin &clock, Color from, Color to, float speed, uint32_t time = kNoColorChange, Color factor = 0xffffffU) :
            Animation(clock, from),
            _from(from),
            _to(to),
            _factor(factor),
            _time(time),
            _waitTimer(0),
            _speed(_secondsToSpeed(speed)),
            _progress(0),
            _finished(false),
            _waiting(false)
        {
        }

        virtual void begin() override
        {
            _loopTimer = millis();
        }

        virtual void setColor(Color color) override
        {
            _to = color;
            _progress = 0;
            _waiting = false;
            _finished = false;
            _loopTimer = millis();
        }

        virtual void loop(uint32_t millisValue)
        {
            if (_finished) {
                return;
            }
            if (_waiting) {
                if (get_time_diff(_waitTimer, millis()) >= _time) {
                    _color = _getColor();
                    _from = _color;
                    _to.rnd();
                    _progress = 0;
                    _waiting = false;
                    _loopTimer = millis();
                }
            }
            else if (get_time_diff(_loopTimer, millis()) >= kUpdateRate) {
                _loopTimer = millis();

                srand(millisValue);
                if (_progress < kMaxProgress) {
                    _progress = std::min<uint32_t>(kMaxProgress, _progress + _speed);
                    float stepRed = (_to.red() - _from.red()) / kFloatMaxProgress;
                    float stepGreen = (_to.green() - _from.green()) / kFloatMaxProgress;
                    float stepBlue = (_to.blue() - _from.blue()) / kFloatMaxProgress;
                    _color = Color(
                        _from.red() + static_cast<uint8_t>(stepRed * _progress),
                        _from.green() + static_cast<uint8_t>(stepGreen * _progress),
                        _from.blue() + static_cast<uint8_t>(stepBlue * _progress)
                    );
                    if (_progress >= kMaxProgress) {
                        _color = _to;
                        if (_time == kNoColorChange) {
                            _finished = true;
                            __LDBG_printf("no color change, waiting");
                            return; // make sure to return after destroying the lambda function
                        }
                        else if (_time == 0) {
                            _from = _to;
                            _to.rnd();
                            _progress = 0;
                            __LDBG_printf("next from=%s to=%s time=%u speed=%u", _from.toString().c_str(), _to.toString().c_str(), _time, _speed);
                        }
                        else {
                            // wait for _time seconds
                            __LDBG_printf("delay=%u wait_timer=%u", _time, _waitTimer + _time);
                            _waiting = true;
                            _waitTimer = millisValue;
                            _setColor(_to);
                        }
                    }
                }
            }
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            display.fill(_color.get());
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            buffer.fill(_color.get());
        }

    private:
        uint16_t _secondsToSpeed(float seconds) const
        {
            return (kProgressPerSecond / seconds) + 0.5;
        }

    private:
        Color _color;
        Color _from;
        Color _to;
        Color _factor;
        uint32_t _time;
        uint32_t _loopTimer;
        uint32_t _waitTimer;
        uint16_t _speed;
        uint16_t _progress;
        bool _finished: 1;
        bool _waiting: 1;
    };

    class SolidColorAnimation : public FadingAnimation {
    public:
        using FadingAnimation::loop;
        using FadingAnimation::copyTo;
        using FadingAnimation::setColor;

        SolidColorAnimation(ClockPlugin &clock, Color color) : FadingAnimation(clock, color, color, 0.75)
        {
        }

        virtual void begin() override
        {
            FadingAnimation::begin();
            _disableBlinkColon = false;
        }
    };

    // ------------------------------------------------------------------------
    // RainbowAnimation

    class RainbowAnimation : public Animation {
    public:
        using RainbowMultiplier = Plugins::ClockConfig::RainbowMultiplier_t;
        using RainbowColor = Plugins::ClockConfig::RainbowColor_t;
    public:
        RainbowAnimation(ClockPlugin &clock, uint16_t speed, RainbowMultiplier multiplier, RainbowColor color) :
            Animation(clock),
            _speed(speed),
            _multiplier(multiplier),
            _color(color),
            _factor(color.factor.value),
            _lastUpdate(0)
        {
        }

        void begin()
        {
            _disableBlinkColon = false;
            _lastUpdate = 0;
            // setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {

            // });
        }

        virtual void loop(uint32_t millisValue) override
        {
            if (_lastUpdate == 0) {
                _lastUpdate = millisValue;
                return;
            }
            if (get_time_diff(_lastUpdate, millisValue) < 25) {
                return;
            }
            _lastUpdate = millisValue;

            _increment(&_multiplier.value, _multiplier.min, _multiplier.max, &_multiplier.incr);
            _increment(&_factor._red, _color.min.red, 256, &_color.red_incr);
            _increment(&_factor._green, _color.min.green, 256, &_color.green_incr);
            _increment(&_factor._blue, _color.min.blue, 256, &_color.blue_incr);
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            Color color;
            for (PixelAddressType i = 0; i < display.kNumPixels; i++) {
                uint32_t ind = (i * _multiplier.value) + (millisValue / _speed);
                uint8_t indMod = (ind % _mod);
                uint8_t idx = (indMod / _divMul);
                float factor1 = 1.0f - ((float)(indMod - (idx * _divMul)) / _divMul);
                float factor2 = (float)((int)(ind - (idx * _divMul)) % _mod) / _divMul;
                switch(idx) {
                    case 0:
                        color = _normalizeColor(_factor.red() * factor1, _factor.green() * factor2, 0);
                        break;
                    case 1:
                        color = _normalizeColor(0, _factor.green() * factor1, _factor.blue() * factor2);
                        break;
                    case 2:
                        color = _normalizeColor(_factor.red() * factor2, 0, _factor.blue() * factor1);
                        break;
                }
                CoordinateType row = i % kRows;
                CoordinateType col = i / kRows;
                if (i % 2 == 1) {
                    row = (kRows - 1) - row;
                }
                display.setPixel(display.getAddress(row,col), color.get());
            }
        }

        void update(uint16_t speed, RainbowMultiplier multiplier, RainbowColor color)
        {
            _speed = speed;
            _multiplier = multiplier;
            _color = color;
        }

    private:
        Color _normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const;
        void _increment(float *valuePtr, float min, float max, float *incrPtr);

        uint16_t _speed;
        RainbowMultiplier _multiplier;
        RainbowColor _color;

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

    inline Color RainbowAnimation::_normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const
    {
        return Color(
            std::max(red, _color.min.red),
            std::max(green, _color.min.green),
            std::max(blue, _color.min.blue)
        );
    }

    inline void RainbowAnimation::_increment(float *valuePtr, float min, float max, float *incrPtr)
    {
        float value;
        float incr;
        // we need to use memcpy in case the pointer is not dword aligned
        memcpy(&value, valuePtr, sizeof(value));
        memcpy(&incr, incrPtr, sizeof(incr));
        if (incr) {
            value += incr;
            if (value < min && incr < 0) {
                value = min;
                incr = -incr;
            }
            else if (value >= max && incr > 0) {
                incr = -incr;
                value = max + incr;
            }
        }
        memcpy(valuePtr, &value, sizeof(*valuePtr));
        memcpy(incrPtr, &incr, sizeof(*incrPtr));
    }

    // ------------------------------------------------------------------------
    // FlashingAnimation

    class FlashingAnimation : public Animation {
    public:
        FlashingAnimation(ClockPlugin &clock, Color color, uint16_t time, uint8_t mod = 2) :
            Animation(clock, color),
            _time(time),
            _mod(mod)
        {
            __LDBG_printf("color=%s time=%u", _color.toString().c_str(), _time);
            _disableBlinkColon = true;
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            display.fill(((millisValue / _time) % _mod == 0) ? _color : CRGB(0));
        }

        virtual void setColor(Color color) override
        {
            _color = color;
        }

    private:
        uint16_t _time;
        uint8_t _mod;
    };

    // ------------------------------------------------------------------------
    // InterleavedAnimation
    // TODO finish InterleavedAnimation

    class InterleavedAnimation : public Animation {
    public:
        InterleavedAnimation(ClockPlugin &clock, Color color, uint16_t row, uint16_t col, uint32_t time) :
            Animation(clock, color),
            _time(time),
            _row(row),
            _col(col)
        {
        }

        virtual bool blend(Animation *animation, DisplayType &display, uint16_t timeLeft, uint32_t millisValue) override
        {
            return false;
        }

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
            Line &operator==(const Line &) = delete;

            Line(Line &&move) :
                _num(std::exchange(move._num, 0)),
                _heat(std::exchange(move._heat, nullptr))
            {
            }

            Line &operator==(Line &&move)
            {
                _heat = std::exchange(move._heat, nullptr);
                _num = std::exchange(move._num, 0);
                return *this;
            }

            Line() : _num(0), _heat(nullptr)
            {
            }

            Line(CoordinateType num, uint8_t *heat) : _num(num), _heat(heat)
            {
                if (!_heat) {
                    _num = 0;
                }
            }

            void init(uint8_t *buffer, CoordinateType num)
            {
                _heat = buffer;
                _num = num;
                std::fill_n(_heat, _num, 0);
            }

            void cooldown(uint8_t cooling)
            {
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

            void heatup()
            {
                // Step 2.  Heat from each cell drifts 'up' and diffuses a little
                for(uint16_t k = _num - 1; k >= 2; k--) {
                    _heat[k] = (_heat[k - 1] + _heat[k - 2] + _heat[k - 2]) / 3;
                }
            }

            void ignite(uint8_t sparking)
            {
                // Step 3.  Randomly ignite new 'sparks' near the bottom
                auto n = std::max<uint8_t>(_num / 5, 2);
                if (random(255) < sparking) {
                    uint8_t y = rand() % n;
                    _heat[y] += (rand() % (255 - 160)) + 160;
                    //heat[y] = random(160, 255);
                }
            }

            uint16_t getNum() const
            {
                return _num;
            }

            Color getHeatColor(uint16_t num, ColorType factor)
            {
                if (num >= _num) {
                    return 0U;
                }
                // Step 4.  Convert heat to LED colors

                // Scale 'heat' down from 0-255 to 0-191
                uint8_t t192 = _heat[num] * (uint16_t)191 / 255;
                // uint8_t t192 = round((_heat[num] / 255.0) * 191);

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
            CoordinateType _num;
            uint8_t *_heat;
        };

    public:
        FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg) :
            Animation(clock),
            _lineCount((cfg.cast_enum_orientation(cfg.orientation) == Orientation::VERTICAL) ? kCols : kRows),
            _lines(new Line[_lineCount]),
            _lineBuffer(new uint8_t[kNumPixels]),
            _cfg(cfg)
        {
            _disableBlinkColon = false;
            if (!_lines || !_lineBuffer) {
                _lineCount = 0;
            }
            else {
                for(uint16_t i = 0; i < _lineCount; i++) {
                    if (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) {
                        _lines[i].init(&_lineBuffer[i * kRows], kRows);
                    }
                    else {
                        _lines[i].init(&_lineBuffer[i * kCols], kCols);

                    }
                    // _lines[i].init(_lineBuffer[] (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) ? kRows : kCols);
                }
            }
        }

        ~FireAnimation() {
            __LDBG_printf("end lines=%u", _lineCount);
            if (_lines) {
                delete[] _lines;
            }
            if (_lineBuffer) {
                delete[] _lineBuffer;
            }
        }

        virtual void begin() override
        {
            _loopTimer = millis();
            _updateRate = std::max<uint16_t>(5, _cfg.speed);
            __LDBG_printf("begin lines=%u update_rate=%u", _lineCount, _updateRate);
        }

        virtual void loop(uint32_t millisValue) override
        {
            if (get_time_diff(_loopTimer, millisValue) >= _updateRate) {
                _loopTimer = millisValue;
                // update all lines
                srand(millisValue);
                for(uint16_t i = 0; i < _lineCount; i++) {
                    _lines[i].cooldown(_cfg.cooling);
                    _lines[i].heatup();
                    _lines[i].ignite(_cfg.sparking);
                }
            }
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        void _copyTo(_Ta &output, uint32_t millisValue) override
        {
            uint8_t mapping = ((_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL ? 2 : 0) + (_cfg.invert_direction ? 1 : 0));
            for(CoordinateType i = 0; i < kRows; i++) {
                auto &line = _lines[i];
                for(CoordinateType j = 0; j < kCols; j++) {
                    auto color = line.getHeatColor(j, _cfg.factor);
                    auto coords = PixelCoordinatesType(i, j);
                    switch(mapping) {
                        case 1:
                            coords.invertColumn();
                            break;
                        case 2:
                            coords.rotate();
                            break;
                        case 3:
                            coords.rotate();
                            coords.invertColumn();
                            break;
                        case 0:
                        default:
                            break;
                    }
                    output.setPixel(coords, color.get());
                }
            }

            // setAnimationCallback([this](PixelAddressType address, ColorType color, const SevenSegmentDisplay::Params_t &params) -> ColorType {
            //     auto pixel = translateAddress(address);
            //     if (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) {
            //         pixel.rotate90();
            //     }
            //     return SevenSegmentDisplay::_adjustBrightnessAlways(_lines[pixel.getRow()].getHeatColor((_cfg.invert_direction) ? pixel.getInvertedCol() : pixel.getCol()), params.brightness());
            // });

        }

    private:
        uint32_t _loopTimer;
        uint16_t _updateRate;
        uint16_t _lineCount;
        Line *_lines;
        uint8_t *_lineBuffer;
        FireAnimationConfig &_cfg;
    };

    // // ------------------------------------------------------------------------
    // // Callback

    // class CallbackAnimation : public Animation {
    // public:
    //     CallbackAnimation(ClockPlugin &clock, AnimationCallback callback, uint16_t updateRate = 20, bool blinkColon = true);

    //     virtual void begin() override;
    //     virtual void end() override;
    //     virtual void loop(uint32_t millisValue) {}

    // private:
    //     AnimationCallback _callback;
    //     uint16_t _updateRate;
    //     bool _blinkColon;
    // };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
