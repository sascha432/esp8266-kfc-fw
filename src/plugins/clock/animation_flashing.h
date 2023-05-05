/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

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

}
