/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // FireField

    // heat based fire simulation, shared by the fire animation and the audio reactive visualizer
    // adapted from an example in FastLED, which is adapted from work done by Mark Kriegsman (called Fire2012).

    class FireField {
    public:
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

            // adds heat to the bottom cells, used by the audio reactions
            void addHeat(uint8_t value)
            {
                if (!value || !_num || !_heat) {
                    return;
                }
                auto n = std::max<uint8_t>(_num / 5, 2);
                for(uint8_t i = 0; i < n; i++) {
                    // the injected heat decreases towards the top of the ignition area
                    auto add = (uint16_t)value * (n - i) / n;
                    auto heat = (uint16_t)_heat[i] + add;
                    _heat[i] = heat > 255 ? 255 : (uint8_t)heat;
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
                uint32_t col;
                if (t192 > 0x80) {                     // hottest
                    col = Color(255, 255, heatramp);
                }
                else if (t192 > 0x40) {             // middle
                    col = Color(255, heatramp, 0);
                }
                else {
                    col = Color(heatramp, 0, 0);
                }
                if (factor) {
                    col =
                        (blend8(col >> 16, 0xff, factor >> 16) << 16) |
                        (blend8(col >> 8, 0xff, factor >> 8) << 8) |
                        (blend8(col, 0xff, factor));
                }
                return col;
            }

        private:
            CoordinateType _num;
            uint8_t *_heat;
        };

    public:
        FireField() : _lineCount(0), _lines(nullptr), _lineBuffer(nullptr)
        {
        }

        ~FireField()
        {
            free();
        }

        FireField(const FireField &) = delete;
        FireField &operator=(const FireField &) = delete;

        // allocates the heat buffer, lineCount lines with lineLength cells
        bool init(uint16_t lineCount, uint16_t lineLength)
        {
            free();
            if (!lineCount || !lineLength) {
                return false;
            }
            _lineCount = lineCount;
            _lines = new Line[lineCount + 1];
            _lineBuffer = new uint8_t[lineCount * lineLength + 1];
            if (!_lines || !_lineBuffer) {
                free();
                return false;
            }
            for(uint16_t i = 0; i < lineCount; i++) {
                _lines[i].init(&_lineBuffer[i * lineLength], lineLength);
            }
            return true;
        }

        void free()
        {
            if (_lines) {
                delete[] _lines;
                _lines = nullptr;
            }
            if (_lineBuffer) {
                delete[] _lineBuffer;
                _lineBuffer = nullptr;
            }
            _lineCount = 0;
        }

        bool isValid() const
        {
            return _lineCount != 0;
        }

        uint16_t getLineCount() const
        {
            return _lineCount;
        }

        // advances the simulation, cooling and sparking are 0-255
        void update(uint32_t millisValue, uint8_t cooling, uint8_t sparking)
        {
            srand(millisValue);
            for(uint16_t i = 0; i < _lineCount; i++) {
                _lines[i].cooldown(cooling);
                _lines[i].heatup();
                _lines[i].ignite(sparking);
            }
        }

        // adds extra heat to the bottom of a line, used by the audio reactions
        void addHeat(uint16_t line, uint8_t value)
        {
            if (line < _lineCount) {
                _lines[line].addHeat(value);
            }
        }

        template<typename _Ta>
        void copyTo(_Ta &output, uint8_t mapping, ColorType factor)
        {
            for(uint16_t i = 0; i < _lineCount; i++) {
                auto &line = _lines[i];
                for(uint16_t j = 0; j < line.getNum(); j++) {
                    auto color = line.getHeatColor(j, factor);
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
        uint16_t _lineCount;
        Line *_lines;
        uint8_t *_lineBuffer;
    };

    // ------------------------------------------------------------------------
    // FireAnimation

    class FireAnimation : public Animation {
    public:
        using FireAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::FireAnimationType;
        using Orientation = FireAnimationConfig::OrientationType;

    public:
        FireAnimation(ClockPlugin &clock, FireAnimationConfig &cfg) :
            Animation(clock),
            _cfg(cfg)
        {
            _disableBlinkColon = false;
            auto vertical = (_cfg.getOrientation() == Orientation::VERTICAL);
            if (!_fire.init(vertical ? getCols() : getRows(), vertical ? getRows() : getCols())) {
                __LDBG_printf("allocating the fire buffer failed");
            }
        }

        ~FireAnimation()
        {
            __LDBG_printf("end lines=%u", _fire.getLineCount());
        }

        virtual void begin() override
        {
            _loopTimer = millis();
            _updateRate = std::max<uint16_t>(5, _cfg.speed);
            __LDBG_printf("begin lines=%u update_rate=%u", _fire.getLineCount(), _updateRate);
        }

        virtual void loop(uint32_t millisValue) override
        {
            if (get_time_since(_loopTimer, millisValue) >= _updateRate) {
                _loopTimer = millisValue;
                // update all lines
                _fire.update(millisValue, _cfg.cooling, _cfg.sparking);
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
            uint8_t mapping = ((_cfg.getOrientation() == Orientation::VERTICAL ? 2 : 0) + (_cfg.invert_direction));
            _fire.copyTo(output, mapping, _cfg.factor);
        }

    private:
        uint32_t _loopTimer;
        uint16_t _updateRate;
        FireField _fire;
        FireAnimationConfig &_cfg;
    };

}
