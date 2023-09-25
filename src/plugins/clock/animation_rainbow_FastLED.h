/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // RainbowAnimationFastLED

    class RainbowAnimationFastLED : public Animation {
    public:
        RainbowAnimationFastLED(ClockPlugin &clock, uint8_t bpm, uint8_t hue, bool inv_dir) :
            Animation(clock),
            _bpm(bpm),
            _hue(inv_dir ? ~hue : hue)
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

    private:
        template<typename _Ta>
        void _fill_rainbow(_Ta &display, uint8_t initialHue, uint8_t deltaHue)
        {
            CHSV hsv;
            hsv.hue = initialHue;
            hsv.val = 255;
            hsv.sat = 240;
            for(uint16_t i = 0; i < display.getNumPixels(); ++i) {
                CoordinateType col = i / getRows();
                CoordinateType row = i % getRows();
                display.pixels(display.getAddress(row, col)) = CRGB(hsv);
                hsv.hue += deltaHue;
            }
        }

    public:
        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            // fill_rainbow(reinterpret_cast<CRGB *>(display.begin()), display.getNumPixels(), beat8(_bpm, 255), _hue);
            _fill_rainbow(display, beat8(_bpm, 255), _hue);
        }

        void update(uint8_t bpm, uint8_t hue, bool inv_dir)
        {
            _bpm = bpm;
            _hue = inv_dir ? ~hue : hue;
        }

    private:
        uint8_t _bpm;
        uint8_t _hue;
    };

}
