/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // PlasmaAnimation

    class PlasmaAnimation : public Animation {
    public:
        using PlasmaAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::PlasmaAnimationType;

    public:
        PlasmaAnimation(ClockPlugin &clock, Color color, PlasmaAnimationConfig &cfg) :
            Animation(clock, color),
            _cfg(cfg)
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
        void _copyTo(_Ta &output, uint32_t millisValue);

    private:
        int8_t _readSineTab(uint8_t ofs);

    private:
        PlasmaAnimationConfig &_cfg;

        static constexpr float radius1 = 65.2, radius2 = 92.0, radius3 = 163.2, radius4 = 176.8, centerX1 = 64.4, centerX2 = 46.4, centerX3 = 93.6, centerX4 = 16.4, centerY1 = 34.8, centerY2 = 26.0, centerY3 = 56.0, centerY4 = -11.6;
    };

}
