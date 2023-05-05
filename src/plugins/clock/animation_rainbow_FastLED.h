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

        // the first and last pixel in a circle might not have a smooth transition due to its 8 bit restriction.
        // the offset allows to move them to another position and hide it. this is way faster than using a more
        // precise calculation. the regular rainbow animation is using float and does not have this issue, but is
        // way slower.
        #if IOT_LED_MATRIX_FASTLED_RAINBOW_OFS

            void _fill_rainbow(struct CRGB * pFirstLED, int numToFill, uint8_t initialhue, uint8_t deltahue)
            {
                CHSV hsv;
                hsv.hue = initialhue;
                hsv.val = 255;
                hsv.sat = 240;
                auto end = IOT_LED_MATRIX_FASTLED_RAINBOW_OFS + numToFill;
                for(int i = IOT_LED_MATRIX_FASTLED_RAINBOW_OFS; i < end; ++i) {
                    pFirstLED[i % numToFill] = hsv;
                    hsv.hue += deltahue;
                }
            }

        #else

            void _fill_rainbow(struct CRGB *pFirstLED, int numToFill, uint8_t initialhue, uint8_t deltahue) {
                fill_rainbow(pFirstLED, numToFill, initialhue, deltahue);
            }

        #endif

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {
            _fill_rainbow(reinterpret_cast<CRGB *>(display.begin()), display.getNumPixels(), beat8(_bpm, 255), _hue);
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
