/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "color.h"
#include "pixel_display.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#define _INCREMENT(value, min, max, incr) \
        if (incr) { \
            value += incr; \
            if (value < min && incr < 0) { \
                value = min; \
                incr = -incr; \
            } \
            else if (value >= max && incr > 0) { \
                incr = -incr; \
                value = max;\
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

        CoordinateType getCols() const;
        CoordinateType getRows() const;
        PixelAddressType getNumPixels() const;

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

        virtual bool hasBlendSupport() const
        {
            return true;
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
        template<typename _Ta>
        DisplayBufferType *getBlendBuffer(_Ta &display)
        {
            if (_buffer) {
                return _buffer;
            }
            auto buf = new DisplayBufferType();
            buf->setSize(display.getRows(), display.getCols());
            return buf;
        }

        // check if the buffer is _buffer and delete it, if not
        void removeBlendBuffer(DisplayBufferType *buffer)
        {
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

        void _freeBuffer(DisplayBufferType *newBuffer = nullptr)
        {
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
            _sourceBuffer(source->getBlendBuffer(display)),
            _targetBuffer(nullptr),
            _display(display),
            _timer(0),
            _duration(duration)
        {
        }

        ~BlendAnimation()
        {
            _source->removeBlendBuffer(_sourceBuffer);
            _target->removeBlendBuffer(_targetBuffer);
            // make target the new animation and delete the source
            __LDBG_printf("source=%p target=%p", _source, _target);
            std::swap(_source, _target);
            delete _target;
        }

        void begin()
        {
            _timer = millis();
            // create first frame to get the buffer
            _target->begin();
            uint32_t ms = millis();
            _target->loop(ms);
            _target->nextFrame(ms);
            _targetBuffer = _target->getBlendBuffer(_display);
        }

        void loop(uint32_t millis)
        {
            _target->loop(millis);
        }

        void nextFrame(uint32_t millis)
        {
            _target->nextFrame(millis);
        }

        void copyTo(DisplayType &display, uint32_t millis)
        {
            _target->copyTo(display, millis);
        }

        void copyTo(DisplayBufferType &buffer, uint32_t millis)
        {
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
            ::blend(_sourceBuffer->begin(), _targetBuffer->begin(), display.begin(), display.size(), amount);
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

}

#include "animation_fading.h"

#include "animation_solid.h"

#include "animation_rainbow.h"

#include "animation_flashing.h"

#include "animation_rainbow_FastLED.h"

#include "animation_interleaved.h"

#include "animation_fire.h"

#include "animation_gradient.h"

#include "animation_plasma.h"

#if IOT_LED_MATRIX_ENABLE_VISUALIZER
#    include "animation_visualizer.h"
#endif

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif
