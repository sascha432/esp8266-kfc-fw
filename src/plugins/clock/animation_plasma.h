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

        ~PlasmaAnimation()
        {
            __LDBG_printf("end");
        }

        virtual void begin() override
        {
            _angle1 = _angle2 = _angle3 = _angle4 = 0;
            _hueShift = 0;
            __LDBG_printf("begin");
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
            int x1, x2, x3, x4, y1, y2, y3, y4, sx1, sx2, sx3, sx4;

            sx1 = cos(_angle1) * radius1 + centerX1;
            sx2 = cos(_angle2) * radius2 + centerX2;
            sx3 = cos(_angle3) * radius3 + centerX3;
            sx4 = cos(_angle4) * radius4 + centerX4;
            y1 = sin(_angle1) * radius1 + centerY1;
            y2 = sin(_angle2) * radius2 + centerY2;
            y3 = sin(_angle3) * radius3 + centerY3;
            y4 = sin(_angle4) * radius4 + centerY4;

            CHSV color(0, 255, 255);

            for(CoordinateType y = 0; y < output.getRows(); y++) {
                x1 = sx1;
                x2 = sx2;
                x3 = sx3;
                x4 = sx4;
                for(CoordinateType x = 0; x < output.getCols(); x++) {
                    auto coords = PixelCoordinatesType(y, x);
                    int value = _hueShift
                        + _readSineTab((x1 * x1 + y1 * y1) / _cfg.xDiv)
                        + _readSineTab((x2 * x2 + y2 * y2) / _cfg.xDiv)
                        + _readSineTab((x3 * x3 + y3 * y3) / _cfg.yDiv)
                        + _readSineTab((x4 * x4 + y4 * y4) / _cfg.yDiv);

                    color.hue = (value * _cfg.hueMul) >> 8;
                    output.setPixel(coords, color);

                    x1--;
                    x2--;
                    x3--;
                    x4--;
                }
                y1--;
                y2--;
                y3--;
                y4--;
            }

            _angle1 += (_cfg.angle1 / 10000.0);
            _angle2 -= (_cfg.angle2 / 10000.0);
            _angle3 += (_cfg.angle3 / 10000.0);
            _angle4 -= (_cfg.angle4 / 10000.0);
            _hueShift += (_cfg.hueShift / 100.0);
        }

    private:
        int8_t _readSineTab(uint8_t ofs);

    private:
        PlasmaAnimationConfig &_cfg;
        float _angle1;
        float _angle2;
        float _angle3;
        float _angle4;
        float _hueShift;

        static constexpr float radius1 = 65.2, radius2 = 92.0, radius3 = 163.2, radius4 = 176.8, centerX1 = 64.4, centerX2 = 46.4, centerX3 = 93.6, centerX4 = 16.4, centerY1 = 34.8, centerY2 = 26.0, centerY3 = 56.0, centerY4 = -11.6;
    };

}
