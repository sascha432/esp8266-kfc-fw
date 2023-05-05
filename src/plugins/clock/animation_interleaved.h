/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // InterleavedAnimation
    // TODO finish InterleavedAnimation

    class InterleavedAnimation : public Animation {
    public:
        InterleavedAnimation(ClockPlugin &clock, Color color, uint16_t row, uint16_t col, uint32_t time) :
            Animation(clock, color),
            _time(time),
            _row(row),
            _col(col)
        //     _loopTimer(0)
        {
        }

        virtual void begin() override {
            // _loopTimer = millis();
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
            Color color;
            CoordinateType rPos;
            CoordinateType cPos;
            if (_time) {
                rPos = (millisValue / _time) % kRows;
                cPos = (millisValue / _time) % kCols;
            }
            else {
                rPos = 0;
                cPos = 0;
            }
            for(CoordinateType i = 0; i < kRows; i++) {
                Color rowColor = (_row > 1) ?
                    (i % _row == rPos ? Color() : _color) :
                    (_color);
                for(CoordinateType j = 0; j < kCols; j++) {
                    display.setPixel(i, j, (_col > 1) ?
                        (i % _col == cPos ? Color() : rowColor) :
                        (rowColor)
                    );
                }
            }
        }

    private:
        uint32_t _time;
        uint16_t _row;
        uint16_t _col;
        // uint32_t _loopTimer;
    };

}
