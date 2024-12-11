/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // XmasAnimation


    static const uint32_t XmasAnimationColorPalettes[] PROGMEM = {
        0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0,           // Simple Color Palette
        0xc25f5f, 0xbb051f, 0x3f8f29, 0x056517, 0x1b5300, 0, // Christmas Noel Color Palette
        0xfb4242, 0xa11029, 0xffd97d, 0x63250e, 0x1f400a, 0, // Christmas Day Theme Color Palette
        0x668c6f, 0x7b0a0a, 0xbaa58c, 0xe5d5bb, 0x213c18, 0, // Christmas Decorations Color Palette
        0xdb0404, 0x169f48, 0x8cd4ff, 0xc6efff, 0xffffff, 0, // Christmas Snow Color Palette
        0
    };

    class XmasAnimation : public Animation {
    public:
        using XmasAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::XmasAnimationType;

    public:
        XmasAnimation(ClockPlugin &clock, XmasAnimationConfig &cfg) :
            Animation(clock, 0U),
            _cfg(cfg),
            _numColors(0)
        {
            _disableBlinkColon = false;
            _cfg.speed = _cfg.speed ? _cfg.speed : 1000;

            // find color palette
            auto palette = _cfg.palette;
            auto pos = 0;
            while(palette) {
                // find zero terminator
                uint32_t color = pgm_read_dword(XmasAnimationColorPalettes + pos++);
                while(color) {
                    color = pgm_read_dword(XmasAnimationColorPalettes + pos++);
                }
                palette--;
            }

            // copy palette
            for(;;) {
                uint32_t color = pgm_read_dword(XmasAnimationColorPalettes + pos++);
                if (!color) {
                    break;
                }
                _colors[_numColors++] = color;
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

    private:
        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            const uint32_t tInterval = _cfg.speed * _numColors; // total interval for all pixels
            const uint32_t pInterval = _cfg.speed; // single pixel
            const uint32_t tPos = millisValue % tInterval;
            const uint32_t pPos = millisValue % pInterval;
            uint32_t colorIdx = tPos / pInterval; // get first color

            // LED pixel grouping
            uint32_t pixels = 1;
            uint32_t gap = 0;

            // start outside the view and move one pixel each time the color changes
            auto prevColor = Color();
            Color color;
            int32_t i = -(((millisValue * (_cfg.pixels + _cfg.gap) / _cfg.speed) % (_cfg.pixels + _cfg.gap)) + (_cfg.pixels + 2));
            for (; i < static_cast<int32_t>(display.getNumPixels()); i++) {
                // flag for blending/fading
                bool fade = _cfg.fade;
                // multiple LEDs per pixel and gap between
                if (pixels < _cfg.pixels) {
                    pixels++;
                    color = _getColor(colorIdx);
                }
                else if (gap < _cfg.gap) {
                    gap++;
                    color = 0;
                    fade = false;
                }
                else {
                    // new pixel
                    pixels = 1;
                    gap = 0;
                    // update colors
                    prevColor = _getColor(colorIdx);
                    _cfg.invert_direction ? --colorIdx : ++colorIdx;
                    color = _getColor(colorIdx);
                }
                // use white to blend
                if (_cfg.sparkling) {
                    prevColor = ((_cfg.sparkling << 16) / ((100 << 8) + 1)); // 0-255(.99)
                    uint32_t tmp = prevColor; // type cast basically
                    prevColor = (tmp << 16) | (tmp << 8) | (tmp);
                }
                // fading enabled and not a gap?
                if (fade) {
                    uint32_t fadeOut;
                    if (pPos < _cfg.fade) { // fade in
                        fract8 span = (pPos << 8) / (_cfg.fade + 1);
                        color = blend(prevColor, color, span);
                    }
                    else if (pPos >= (fadeOut = (pInterval - _cfg.fade))) { // fade out
                        fract8 span = ((pPos - fadeOut) << 8) / (_cfg.fade + 1);
                        color = blend(color, prevColor, span);
                    }
                }
                if (i >= 0) {
                    // draw pixel
                    CoordinateType col = i / display.getRows();
                    CoordinateType row = i % display.getRows();
                    display.setPixel(row, col, color);
                }
            }
        }

        inline ColorType _getColor(int colorIdx) {
            return _colors[colorIdx % _numColors];
        }

    private:
        static constexpr auto kMaxColors = 8;

        XmasAnimationConfig &_cfg;
        std::array<ColorType, kMaxColors> _colors;
        uint32_t _numColors;
    };

}
