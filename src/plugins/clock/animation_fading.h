/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

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

}
