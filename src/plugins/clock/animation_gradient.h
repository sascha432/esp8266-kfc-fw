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

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            uint16_t blending = 0;
            if (_speed) {
                blending = (((static_cast<uint16_t>(millis()) << 10) / (0x10000 - _speed)) >> 4) & 0x1ff;
                blending = blending > 0xff ? 0x1ff - blending : blending;
            }
            auto prev = std::prev(_gradient.end());
            auto cur = _gradient.begin();
            auto next = cur;
            #define DEBUG_GRADIENT 0
            #if DEBUG_GRADIENT
                static uint32_t timer;
                bool displayDebug = false;
                if (millis() - timer > 100) {
                    timer = millis();
                    displayDebug = true;
                }
            #endif
            while(cur != _gradient.end()) {
                if (++next == _gradient.end()) {
                    next = _gradient.begin();
                }
                auto pos = std::min<PixelAddressType>(cur->_pixel, display.getNumPixels() - 1);
                auto num = std::min<int>(next->_pixel, display.getNumPixels() - 1) - pos + 1;

                if (num > 0 && (num + pos) <= static_cast<int>(display.getNumPixels())) {
                    auto from = blend(cur->_color, prev->_color, blending);
                    auto to = blend(next->_color, cur->_color, blending);

                    #if DEBUG_GRADIENT
                        if (displayDebug) {
                            __DBG_printf("prev=%u/%s cur=%u/%s next=%u/%s pos=%u num=%d speed=%u blending=%u gradient=%s-%s",
                                prev->_pixel, Color(prev->_color).toString().c_str(),
                                cur->_pixel, Color(cur->_color).toString().c_str(),
                                next->_pixel, Color(next->_color).toString().c_str(),
                                pos, num, _speed, blending,
                                Color(from).toString().c_str(),
                                Color(to).toString().c_str()
                            );
                        }
                    #endif

                    fill_gradient_RGB(reinterpret_cast<CRGB *>(display.begin()) + pos, num, from, to);
                }
                prev = cur;
                ++cur;
            }
            #undef DEBUG_GRADIENT
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
