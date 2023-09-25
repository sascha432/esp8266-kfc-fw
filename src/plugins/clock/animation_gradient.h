/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {


    // ------------------------------------------------------------------------
    // GradientAnimation

    class GradientAnimation : public Animation {
    public:
        struct GradientPixel {
            PixelAddressType _pixel;
            CRGB _color;
            GradientPixel(uint16_t pixel, uint32_t color) : _pixel(static_cast<PixelAddressType>(pixel)), _color(color) {
            }
        };

        using GradientAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::GradientAnimationType;
        using GradientVector = std::vector<GradientPixel>;

    public:
        GradientAnimation(ClockPlugin &clock, const GradientAnimationConfig &config) :
            Animation(clock)
        {
            update(config);
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

    private:
        template<typename _Ta>
        void fill_gradient_RGB(_Ta &display, uint16_t pos, uint16_t startpos, CRGB startcolor, uint16_t endpos, CRGB endcolor)
        {
            // if the points are in the wrong order, straighten them
            if( endpos < startpos) {
                uint16_t t = endpos;
                CRGB tc = endcolor;
                endcolor = startcolor;
                endpos = startpos;
                startpos = t;
                startcolor = tc;
            }

            saccum87 rdistance87;
            saccum87 gdistance87;
            saccum87 bdistance87;

            rdistance87 = (endcolor.r - startcolor.r) << 7;
            gdistance87 = (endcolor.g - startcolor.g) << 7;
            bdistance87 = (endcolor.b - startcolor.b) << 7;

            uint16_t pixeldistance = endpos - startpos;
            int16_t divisor = pixeldistance ? pixeldistance : 1;

            saccum87 rdelta87 = rdistance87 / divisor;
            saccum87 gdelta87 = gdistance87 / divisor;
            saccum87 bdelta87 = bdistance87 / divisor;

            rdelta87 *= 2;
            gdelta87 *= 2;
            bdelta87 *= 2;

            accum88 r88 = startcolor.r << 8;
            accum88 g88 = startcolor.g << 8;
            accum88 b88 = startcolor.b << 8;
            startpos += pos;
            endpos += pos;
            for(uint16_t i = startpos; i <= endpos; ++i) {
                CoordinateType col = i / display.getRows();
                CoordinateType row = i % display.getRows();
                display.pixels(display.getAddress(row, col)) = CRGB( r88 >> 8, g88 >> 8, b88 >> 8);;
                r88 += rdelta87;
                g88 += gdelta87;
                b88 += bdelta87;
            }
        }

        template<typename _Ta>
        void fill_gradient_RGB(_Ta &display, uint16_t pos, uint16_t numLeds, const CRGB& c1, const CRGB& c2)
        {
            uint16_t last = numLeds - 1;
            fill_gradient_RGB(display, pos, 0, c1, last, c2);
        }

        template<typename _Ta>
        void fill_gradient_RGB(_Ta &display, uint16_t pos, uint16_t numLeds, const CRGB& c1, const CRGB& c2, const CRGB& c3)
        {
            uint16_t half = (numLeds / 2);
            uint16_t last = numLeds - 1;
            fill_gradient_RGB(display, pos,    0, c1, half, c2);
            fill_gradient_RGB(display, pos, half, c2, last, c3);
        }

        template<typename _Ta>
        void fill_gradient_RGB(_Ta &display, uint16_t pos, uint16_t numLeds, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4)
        {
            uint16_t onethird = (numLeds / 3);
            uint16_t twothirds = ((numLeds * 2) / 3);
            uint16_t last = numLeds - 1;
            fill_gradient_RGB(display, pos,         0, c1,  onethird, c2);
            fill_gradient_RGB(display, pos,  onethird, c2, twothirds, c3);
            fill_gradient_RGB(display, pos, twothirds, c3,      last, c4);
        }

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            fract8 blending = 0;
            if (_speed) {
                auto tmp = (millisValue / _speed) % 512;
                if (tmp > 255) {
                    tmp = 511 - tmp;
                }
                blending = tmp;
            }
            auto prev = std::prev(_gradient.end());
            auto cur = _gradient.begin();
            auto next = cur;
            while(cur != _gradient.end()) {
                if (++next == _gradient.end()) {
                    next = _gradient.begin();
                }
                auto pos = std::min<PixelAddressType>(cur->_pixel, display.getNumPixels() - 1);
                auto num = std::min<int>(next->_pixel, display.getNumPixels() - 1) - pos + 1;

                if (num > 0 && (num + pos) <= static_cast<int>(display.getNumPixels())) {
                    auto from = blend(cur->_color, prev->_color, blending);
                    auto to = blend(next->_color, cur->_color, blending);
                    // fill_gradient_RGB(reinterpret_cast<CRGB *>(display.begin()) + pos, num, from, to);
                    fill_gradient_RGB(display, pos, num, from, to);
                }
                prev = cur;
                ++cur;
            }
        }

        void update(const GradientAnimationConfig &config)
        {
            _gradient.clear();
            // config.entries must be sorted by pixel in ascending order
            for(uint8_t i = 0; i < config.kMaxEntries; i++) {
                if (config.entries[i].pixel != config.kDisabled) {
                    _gradient.emplace_back(config.entries[i].pixel, config.entries[i].color);
                }
            }
            _speed = config.speed;
        }

    private:
        GradientVector _gradient;
        uint16_t _speed;
    };

}
