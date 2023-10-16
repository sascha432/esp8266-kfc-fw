/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // FireAnimation

    class FireAnimation : public Animation {
    public:
        // adapted from an example in FastLED, which is adapted from work done by Mark Kriegsman (called Fire2012).
        using FireAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::FireAnimationType;
        using Orientation = FireAnimationConfig::OrientationType;

    private:
        class Line {
        public:

            Line(const Line &) = delete;
            Line &operator==(const Line &) = delete;

            Line(Line &&move) :
                _num(std::exchange(move._num, 0)),
                _heat(std::exchange(move._heat, nullptr))
            {
            }

            Line &operator==(Line &&move)
            {
                _heat = std::exchange(move._heat, nullptr);
                _num = std::exchange(move._num, 0);
                return *this;
            }

            Line() : _num(0), _heat(nullptr)
            {
            }

            Line(CoordinateType num, uint8_t *heat) : _num(num), _heat(heat)
            {
                if (!_heat) {
                    _num = 0;
                }
            }

            void init(uint8_t *buffer, CoordinateType num)
            {
                _heat = buffer;
                _num = num;
                std::fill_n(_heat, _num, 0);
            }

            void cooldown(uint8_t cooling)
            {
                // Step 1.  Cool down every cell a little
                for (uint16_t i = 0; i < _num; i++) {
                    uint8_t cooldownValue = rand() % (((cooling * 10) / _num) + 2);
                    if (cooldownValue > _heat[i]) {
                        _heat[i] = 0;
                    }
                    else {
                        _heat[i] -= cooldownValue;
                    }
                }
            }

            void heatup()
            {
                // Step 2.  Heat from each cell drifts 'up' and diffuses a little
                for(uint16_t k = _num - 1; k >= 2; k--) {
                    _heat[k] = (_heat[k - 1] + _heat[k - 2] + _heat[k - 2]) / 3;
                }
            }

            void ignite(uint8_t sparking)
            {
                // Step 3.  Randomly ignite new 'sparks' near the bottom
                auto n = std::max<uint8_t>(_num / 5, 2);
                if (random(255) < sparking) {
                    uint8_t y = rand() % n;
                    _heat[y] += (rand() % (255 - 160)) + 160;
                    //heat[y] = random(160, 255);
                }
            }

            uint16_t getNum() const
            {
                return _num;
            }

            Color getHeatColor(uint16_t num, ColorType factor)
            {
                if (num >= _num) {
                    return 0U;
                }
                // Step 4.  Convert heat to LED colors

                // Scale 'heat' down from 0-255 to 0-191
                uint8_t t192 = _heat[num] * (uint16_t)191 / 255;
                // uint8_t t192 = round((_heat[num] / 255.0) * 191);

                // calculate ramp up from
                uint8_t heatramp = t192 & 0x3F; // 0..63
                heatramp <<= 2; // scale up to 0..252

                // figure out which third of the spectrum we're in:
                if (t192 > 0x80) {                     // hottest
                    return Color(255, 255, heatramp);
                }
                else if (t192 > 0x40) {             // middle
                    return Color(255, heatramp, 0);
                }
                return Color(heatramp, 0, 0);
            }

        private:
            CoordinateType _num;
            uint8_t *_heat;
        };

    public:
        FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg) :
            Animation(clock),
            _lineCount((cfg.cast_enum_orientation(cfg.orientation) == Orientation::VERTICAL) ? getCols() : getRows()),
            _lines(new Line[_lineCount]),
            _lineBuffer(new uint8_t[getNumPixels()]),
            _cfg(cfg)
        {
            _disableBlinkColon = false;
            if (!_lines || !_lineBuffer) {
                _lineCount = 0;
            }
            else {
                for(uint16_t i = 0; i < _lineCount; i++) {
                    if (_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL) {
                        _lines[i].init(&_lineBuffer[i * getRows()], getRows());
                    }
                    else {
                        _lines[i].init(&_lineBuffer[i * getCols()], getCols());
                    }
                }
            }
        }

        ~FireAnimation() {
            __LDBG_printf("end lines=%u", _lineCount);
            if (_lines) {
                delete[] _lines;
            }
            if (_lineBuffer) {
                delete[] _lineBuffer;
            }
        }

        virtual void begin() override
        {
            _loopTimer = millis();
            _updateRate = std::max<uint16_t>(5, _cfg.speed);
            __LDBG_printf("begin lines=%u update_rate=%u", _lineCount, _updateRate);
        }

        virtual void loop(uint32_t millisValue) override
        {
            if (get_time_since(_loopTimer, millisValue) >= _updateRate) {
                _loopTimer = millisValue;
                // update all lines
                srand(millisValue);
                for(uint16_t i = 0; i < _lineCount; i++) {
                    _lines[i].cooldown(_cfg.cooling);
                    _lines[i].heatup();
                    _lines[i].ignite(_cfg.sparking);
                }
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

        template<typename _Ta>
        void _copyTo(_Ta &output, uint32_t millisValue)
        {
            uint8_t mapping = ((_cfg.cast_enum_orientation(_cfg.orientation) == Orientation::VERTICAL ? 2 : 0) + (_cfg.invert_direction));
            for(uint16_t i = 0; i < _lineCount; i++) {
                auto &line = _lines[i];
                for(uint16_t j = 0; j < line.getNum(); j++) {
                    auto color = line.getHeatColor(j, _cfg.factor);
                    auto coords = PixelCoordinatesType(i, j);
                    switch(mapping) {
                        case 1:
                            coords.invertColumn(output.getCols());
                            break;
                        case 2:
                            coords.rotate();
                            break;
                        case 3:
                            coords.rotate();
                            coords.invertRow(output.getRows());
                            break;
                        default:
                            break;
                    }
                    output.setPixel(coords, color.get());
                }
            }
        }

    private:
        uint32_t _loopTimer;
        uint16_t _updateRate;
        uint16_t _lineCount;
        Line *_lines;
        uint8_t *_lineBuffer;
        FireAnimationConfig &_cfg;
    };

}
