/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include "pixel_color.h"

namespace Clock {

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

}
