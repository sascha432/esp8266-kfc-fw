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


#define _INCREMENT(value, min, max, incr) \
        if (incr) { \
            value += incr; \
            if (value < min && incr < 0) { \
                value = min; \
                incr = -incr; \
            } \
            else if (value >= max && incr > 0) { \
                incr = -incr; \
                value = max + incr;\
            } \
        }

namespace Clock {

    using DisplayBufferType = Clock::PixelDisplayBuffer<
        IOT_LED_MATRIX_PIXEL_OFFSET,
        IOT_LED_MATRIX_ROWS,
        IOT_LED_MATRIX_COLS,
        IOT_LED_MATRIX_OPTS_REVERSE_ROWS,
        IOT_LED_MATRIX_OPTS_REVERSE_COLS,
        IOT_LED_MATRIX_OPTS_ROTATE,
        IOT_LED_MATRIX_OPTS_INTERLEAVED
    >;

#if IOT_LED_MATRIX

    using DisplayType = Clock::PixelDisplay<
            Clock::NeoPixelController<IOT_LED_MATRIX_OUTPUT_PIN>,
            Clock::PixelDisplayBuffer<
                IOT_LED_MATRIX_PIXEL_OFFSET,
                IOT_LED_MATRIX_ROWS,
                IOT_LED_MATRIX_COLS,
                IOT_LED_MATRIX_OPTS_REVERSE_ROWS,
                IOT_LED_MATRIX_OPTS_REVERSE_COLS,
                IOT_LED_MATRIX_OPTS_ROTATE,
                IOT_LED_MATRIX_OPTS_INTERLEAVED
            >
        >;

    using CoordinateType = DisplayType::CoordinateType;
    using PixelAddressType = DisplayType::PixelAddressType;
    using PixelCoordinatesType = DisplayType::PixelCoordinatesType;

#else

    using BaseDisplayType = Clock::PixelDisplay<
            Clock::NeoPixelController<IOT_LED_MATRIX_OUTPUT_PIN>,
            Clock::PixelDisplayBuffer<
                IOT_LED_MATRIX_PIXEL_OFFSET,
                IOT_LED_MATRIX_ROWS,
                IOT_LED_MATRIX_COLS,
                IOT_LED_MATRIX_OPTS_REVERSE_ROWS,
                IOT_LED_MATRIX_OPTS_REVERSE_COLS,
                IOT_LED_MATRIX_OPTS_ROTATE,
                IOT_LED_MATRIX_OPTS_INTERLEAVED
            >
        >;

    using CoordinateType = BaseDisplayType::CoordinateType;
    using PixelAddressType = BaseDisplayType::PixelAddressType;
    using PixelCoordinatesType = BaseDisplayType::PixelCoordinatesType;

    #include "seven_segment_display.h"

    using DisplayType = SevenSegment::Display;

    static_assert(IOT_CLOCK_NUM_PIXELS == DisplayType::kNumPixels, "total pixels do not match");

#endif


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
            _buffer(nullptr),
            _disableBlinkColon(kDefaultDisableBlinkColon),
            // _mode(ModeType::NONE),
            _color(color)
        {
        }

        virtual ~Animation()
        {
            _freeBuffer();
        }

        virtual void begin()
        {
        }

        // copy frame into display/buffer
        // if the target buffer is the own _buffer, this step can be skipped
        virtual void copyTo(DisplayType &display, uint32_t millisValue) = 0;
        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) = 0;

        // virtual void copyTo(DisplayType &display, uint32_t millisValue)
        // {
        //     __LDBG_printf("_buffer=%p display=%u", _buffer, &display);
        //     if (hasBuffer()) {
        //         display.copy(*_buffer, display, display.kNumPixels);
        //     }
        // }

        // virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue)
        // {
        //     __LDBG_printf("_buffer=%p buffer=%u", _buffer, &buffer);
        //     if (hasBuffer()) {
        //         buffer.copy(*_buffer, buffer, buffer.kNumPixels);
        //     }
        // }

        virtual void loop(uint32_t millisValue)
        {
        }

        virtual void nextFrame(uint32_t millis)
        {
        }

        // bool hasDisplay() const
        // {
        //     return _display != nullptr;
        // }

        bool hasBuffer() const
        {
            return _buffer != nullptr;
        }

        // void setDisplay(DisplayType *display) {
        //     _freeBuffer();
        //     _display = display;
        // }

        // void setBuffer(DisplayBufferType *buffer) {
        //     _freeBuffer(buffer);
        //     _display = nullptr;
        // }

        // DisplayBufferType *getBuffer() {
        //     return _buffer;
        // }

        // return the buffer or create a new one
        // to avoid allocating another one, at least one frame must be created before calling the method
        DisplayBufferType *getBlendBuffer() {
            if (_buffer) {
                return _buffer;
            }
            return new DisplayBufferType();
        }

        // check if the buffer is _buffer and delete it, if not
        void removeBlendBuffer(DisplayBufferType *buffer) {
            if (buffer != _buffer) {
                delete buffer;
            }
        }

        bool isBlinkColonDisabled() const
        {
            return _disableBlinkColon;
        }

        virtual void setColor(Color color)
        {
            _color = color;
        }

        // void setMode(ModeType mode)
        // {
        //     _mode = mode;
        // }

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
        DisplayBufferType *_buffer;

    protected:
        bool _disableBlinkColon;
        // ModeType _mode;
        Color _color;
    };

    class BlendAnimation {
    public:
        static constexpr uint16_t kDefaultTime = 4000;

    public:
        // source       variable of current animation
        // target       object of new animation
        // display      blend both animations into display
        // duration     time in milliseconds
        BlendAnimation(Animation *&source, Animation *target, DisplayType &display, uint16_t duration) :
            _source(source),
            _target(target),
            _sourceBuffer(source->getBlendBuffer()),
            _targetBuffer(nullptr),
            _display(display),
            _timer(0),
            _duration(duration)
        {
        }

        ~BlendAnimation() {
            _source->removeBlendBuffer(_sourceBuffer);
            _target->removeBlendBuffer(_targetBuffer);
            // make target the new animation and delete the source
            __DBG_printf("source=%p target=%p", _source, _target);
            std::swap(_source, _target);
            delete _target;
        }

        void begin() {
            _timer = millis();
            // create first frame to get the buffer
            _target->begin();
            uint32_t ms = millis();
            _target->loop(ms);
            _target->nextFrame(ms);
            _targetBuffer = _target->getBlendBuffer();
        }

        void loop(uint32_t millis) {
            _target->loop(millis);
        }

        void nextFrame(uint32_t millis) {
            _target->nextFrame(millis);
        }

        void copyTo(DisplayType &display, uint32_t millis) {
            _target->copyTo(display, millis);
        }

        void copyTo(DisplayBufferType &buffer, uint32_t millis) {
            _target->copyTo(buffer, millis);
        }

        bool blend(DisplayType &display, uint32_t millisValue)
        {
            auto timeLeft = get_time_diff(_timer, millisValue);
            if (timeLeft >= _duration) {
                _target->copyTo(_display, millisValue);
                return false;
            }

            _source->copyTo(*_sourceBuffer, millisValue);
            _target->copyTo(*_targetBuffer, millisValue);

            fract8 amount = ((timeLeft * 255) / _duration);
            ::blend(_sourceBuffer->begin(), _targetBuffer->begin(), display.begin(), display.kNumPixels, amount);
            return true;
        }

    private:
        Animation *&_source;
        Animation *_target;
        DisplayBufferType *_sourceBuffer;
        DisplayBufferType *_targetBuffer;
        DisplayType &_display;
        uint32_t _timer;
        uint16_t _duration;
    };

    // ------------------------------------------------------------------------
    // FadingAnimation
    class FadingAnimation : public Animation {
    public:
        static constexpr uint32_t kNoColorChange = ~0;

    public:
        FadingAnimation(ClockPlugin &clock, Color from, Color to, uint16_t durationMillis = 1000, uint32_t holdTimeMillis = kNoColorChange, Color factor = 0xffffffU) :
            Animation(clock, from),
            _to(to),
            _factor(factor),
            _waitTimer(0),
            _holdTime(holdTimeMillis),
            _duration(durationMillis),
            _finished(false),
            _waiting(false)
        {
        }

        virtual void begin() override
        {
            _from = _color;
            _waiting = false;
            _finished = false;
            // __LDBG_printf("fading from %s to %s in %ums", _from.toString().c_str(), _to.toString().c_str(), _duration);
            _loopTimer = millis();
        }

        virtual void setColor(Color color) override
        {
            // __LDBG_printf("set color=%s current=%s", color.toString().c_str(), _color.toString().c_str());
            _to = color;
            begin();
        }

        virtual void loop(uint32_t millisValue)
        {
            if (_waiting && get_time_diff(_waitTimer, millis()) >= _holdTime) {
                // __LDBG_printf("waiting period is over");
                _color = _getColor();
                srand(millisValue);
                _to.rnd();
                begin();
            }
        }

        void _updateColor() {

            if (_waiting || _finished) {
                return;
            }

            auto duration = get_time_diff(_loopTimer, millis());
            if (duration >= _duration) {
                _color = _to;
                if (_holdTime == kNoColorChange) {
                    // __LDBG_printf("no color change activated");
                    // no automatic color change enabled
                    _finished = true;
                    _waiting = false;
                    return;
                }
                // __LDBG_printf("entering waiting period time=%u", _holdTime);
                // wait for next color change
                _waitTimer = millis();
                _waiting = true;
                _finished = false;
                _setColor(_to);
                return;
            }

            fract8 amount = duration * 255 / _duration;
            _color = blend(_from, _to, amount);
        }

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _updateColor();
            display.fill(_color);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _updateColor();
            buffer.fill(_color);
        }

    private:
        Color _color;
        Color _from;
        Color _to;
        Color _factor;
        uint32_t _loopTimer;
        uint32_t _waitTimer;
        uint32_t _holdTime;
        uint16_t _duration;
        bool _finished: 1;
        bool _waiting: 1;
    };

    class SolidColorAnimation : public FadingAnimation {
    public:
        using FadingAnimation::loop;
        using FadingAnimation::copyTo;
        using FadingAnimation::setColor;

        SolidColorAnimation(ClockPlugin &clock, Color color) : FadingAnimation(clock, color, color)
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

            _INCREMENT(_multiplier.value, _multiplier.min, _multiplier.max, _multiplier.incr);
            _INCREMENT(_factor._red, _color.min.red, 256, _color.red_incr);
            _INCREMENT(_factor._green, _color.min.green, 256, _color.green_incr);
            _INCREMENT(_factor._blue, _color.min.blue, 256, _color.blue_incr);
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
            CoordinateType row = 0;
            CoordinateType col = 0;
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
                // CoordinateType row = i % kRows;
                // CoordinateType col = i / kRows;
                // if (i % 2 == 1) {
                //     row = (kRows - 1) - row;
                // }
                display.setPixel(row, col, color);
                row++;
                if (row >= kRows) {
                    row = 0;
                    col++;
                }
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

    // ------------------------------------------------------------------------
    // RainbowAnimationFastLED

    class RainbowAnimationFastLED : public Animation {
    public:
        RainbowAnimationFastLED(ClockPlugin &clock, uint8_t bpm, uint8_t hue) :
            Animation(clock),
            _bpm(bpm),
            _hue(hue)
        {
        }

        void begin()
        {
            _disableBlinkColon = false;
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
            fill_rainbow(reinterpret_cast<CRGB *>(display.begin()), display.getNumPixels(), beat8(_bpm, 255), _hue);
        }

        void update(uint8_t bpm, uint8_t hue)
        {
            _bpm = bpm;
            _hue = hue;
        }

    private:
        uint8_t _bpm;
        uint8_t _hue;
    };

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
            display.fill(((millisValue / _time) % _mod == 0) ? _color : Color());
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
        //     _loopTimer(0)
        {
        }

        virtual void begin() override {
            // _loopTimer = millis();
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
            Color color;
            CoordinateType rPos;
            CoordinateType cPos;
            if (_time) {
                rPos = (millisValue / _time) % kRows;
                cPos = (millisValue / _time) % kCols;
            }
            else {
                rPos = 0;
                cPos = 0;
            }
            for(CoordinateType i = 0; i < kRows; i++) {
                Color rowColor = (_row > 1) ?
                    (i % _row == rPos ? Color() : _color) :
                    (_color);
                for(CoordinateType j = 0; j < kCols; j++) {
                    display.setPixel(i, j, (_col > 1) ?
                        (i % _col == cPos ? Color() : rowColor) :
                        (rowColor)
                    );
                }
            }
        }

    private:
        uint32_t _time;
        uint16_t _row;
        uint16_t _col;
        // uint32_t _loopTimer;
    };

    // ------------------------------------------------------------------------
    // FireAnimation

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
        void _copyTo(_Ta &output, uint32_t millisValue)
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
        }

    private:
        uint32_t _loopTimer;
        uint16_t _updateRate;
        uint16_t _lineCount;
        Line *_lines;
        uint8_t *_lineBuffer;
        FireAnimationConfig &_cfg;
    };

    // ------------------------------------------------------------------------
    // VisualizerAnimation
    //
    // virtual LEDs for led_matrix_visualizer

#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER

    class VisualizerAnimation : public Animation {
    public:
        using VisualizerAnimationConfig = Plugins::ClockConfig::VisualizerAnimation_t;
        using DisplayType = Clock::DisplayType;

    public:
        VisualizerAnimation(ClockPlugin &clock, VisualizerAnimationConfig &cfg) :
            Animation(clock),
            _cfg(cfg),
            _speed(70),
            _gHue(0),
            _iPos(0),
            _lastBrightness(0),
            _lastFinished(true)
        {
            std::fill(std::begin(_incomingPacket), std::end(_incomingPacket), 0);
            std::fill(_lastVals.begin(), _lastVals.end(), 1);

            _disableBlinkColon = true;
            _udp.begin(cfg.port);
        }

        ~VisualizerAnimation() {
            // if (_leds) {
            //     delete[] _leds;
            // }
            // _setLeds(nullptr);
        }

        // void _setLeds(CRGB *leds);

        virtual void begin() override
        {
            _loopTimer = millis();
            _updateRate = 1000 / 120;
            fill_solid(_leds, kNumPixels, CHSV(0, 0, 0));
            __LDBG_printf("begin update_rate=%u pixels=%u", _updateRate, kNumPixels);
        }

        virtual void loop(uint32_t millisValue) override;

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        void _copyTo(_Ta &output, uint32_t millisValue)
        {
            for(CoordinateType i = 0; i < kRows; i++) {
                for(CoordinateType j = 0; j < kCols; j++) {
                    auto coords = PixelCoordinatesType(i, j);
                    output.setPixel(coords, _leds[i * kRows + j]);
                }
            }
        }

        int _getVolume(uint8_t vals[], int start, int end, double factor);
        void _BrightnessVisualizer();
        bool _FadeUp(CRGB c, int start, int end, int update_rate, int starting_brightness, bool isNew);
        void _TrailingBulletsVisualizer();
        void _ShiftLeds(int shiftAmount);
        void _SendLeds(CHSV c, int shiftAmount);
        void _SendTrailingLeds(CHSV c, int shiftAmount);
        CHSV _getVisualizerBulletValue(double cd);
        CHSV _getVisualizerBulletValue(int hue, double cd);
        void _vuMeter(CHSV c, int mode);
        void _vuMeterTriColor();
        int _getPeakPosition();
        void _printPeak(CHSV c, int pos, int grpSize);
        void _setBar(int row, double num, CRGB color, bool fill_ends);
        void _setBar(int row, int num, CRGB color);
        void _setBar(int row, double num, CHSV col, bool fill_ends);
        void _setBar(int row, int num, CHSV col);
        void _setBarDoubleSided(int row, double num, CRGB color, bool fill_ends);
        void _setBarDoubleSided(int row, double num, CHSV col, bool fill_ends);


        bool _parseUdp();

        void _RainbowVisualizer();
        void _RainbowVisualizerDoubleSided();
        void _SingleColorVisualizer();
        void _SingleColorVisualizerDoubleSided();

    private:
        static constexpr int kVisualizerPacketSize = 32;
        static constexpr int kAvgArraySize = 10;
        static constexpr uint8_t kBandStart = 0;
        static constexpr uint8_t kBandEnd = 3;

        uint32_t _loopTimer;
        uint16_t _updateRate;
        VisualizerAnimationConfig &_cfg;
        WiFiUDP _udp;
        CRGB _leds[kNumPixels];
        uint8_t _incomingPacket[kVisualizerPacketSize + 1];
        std::array<uint8_t, kAvgArraySize> _lastVals;
        uint8_t _speed;
        uint8_t _gHue;
        int _iPos;
        int _lastBrightness;
        bool _lastFinished;
        CRGB _lastCol;
    };

#endif

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
