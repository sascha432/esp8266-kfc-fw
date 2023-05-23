/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

namespace Clock {

    // ------------------------------------------------------------------------
    // RainbowAnimation

    class RainbowAnimation : public Animation {
    public:
        using RainbowConfigType = KFCConfigurationClasses::Plugins::ClockConfigNS::RainbowAnimationType;
        using RainbowMultiplierType = RainbowConfigType::MultiplierType;
        using RainbowColorType = RainbowConfigType::ColorAnimationType;

    public:
        RainbowAnimation(ClockPlugin &clock, uint16_t speed, RainbowMultiplierType multiplier, RainbowColorType color) :
            Animation(clock),
            _speed(speed),
            _multiplier(multiplier),
            _color(color),
            _factor(color.factor.value),
            _lastUpdate(0)
        {
        }

        void begin()
        {
            _disableBlinkColon = false;
            _lastUpdate = 0;
        }

        virtual void loop(uint32_t millisValue) override
        {
            if (_lastUpdate == 0) {
                _lastUpdate = millisValue;
                return;
            }
            if (get_time_diff(_lastUpdate, millisValue) < 25) {
                return;
            }
            _lastUpdate = millisValue;

            _INCREMENT(_multiplier.value, _multiplier.min, _multiplier.max, _multiplier.incr);
            _INCREMENT(_factor._red, _color.min.red, 256, _color.red_incr);
            _INCREMENT(_factor._green, _color.min.green, 256, _color.green_incr);
            _INCREMENT(_factor._blue, _color.min.blue, 256, _color.blue_incr);
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
            CoordinateType row = 0;
            CoordinateType col = 0;
            for (PixelAddressType i = 0; i < display.size(); i++) {
                uint32_t ind = (i * _multiplier.value) + (millisValue / _speed);
                uint8_t indMod = (ind % _mod);
                uint8_t idx = (indMod / _divMul);
                float factor1 = 1.0f - ((float)(indMod - (idx * _divMul)) / _divMul);
                float factor2 = (float)((int)(ind - (idx * _divMul)) % _mod) / _divMul;
                switch(idx) {
                    case 0:
                        color = _normalizeColor(_factor.red() * factor1, _factor.green() * factor2, 0);
                        break;
                    case 1:
                        color = _normalizeColor(0, _factor.green() * factor1, _factor.blue() * factor2);
                        break;
                    case 2:
                        color = _normalizeColor(_factor.red() * factor2, 0, _factor.blue() * factor1);
                        break;
                }
                // CoordinateType row = i % kRows;
                // CoordinateType col = i / kRows;
                // if (i % 2 == 1) {
                //     row = (kRows - 1) - row;
                // }
                display.setPixel(row, col, color);
                row++;
                if (row >= kRows) {
                    row = 0;
                    col++;
                }
            }
        }

        void update(uint16_t speed, RainbowMultiplierType multiplier, RainbowColorType color)
        {
            _speed = speed;
            _multiplier = multiplier;
            _color = color;
        }

    private:
        Color _normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const;

        uint16_t _speed;
        RainbowMultiplierType _multiplier;
        RainbowColorType _color;

        struct ColorFactor {
            float _red;
            float _green;
            float _blue;
            ColorFactor(uint32_t value) : _red(value & 0xff), _green((value >> 8) & 0xff), _blue((value >> 16) & 0xff) {}
            float red() const {
                return _red;
            }
            float green() const {
                return _green;
            }
            float blue() const {
                return _blue;
            }
        } _factor;
        uint32_t _lastUpdate;

        static constexpr uint8_t _mod = 120;
        static constexpr uint8_t _divMul = 40;
    };

    inline Color RainbowAnimation::_normalizeColor(uint8_t red, uint8_t green, uint8_t blue) const
    {
        return Color(
            std::max(red, _color.min.red),
            std::max(green, _color.min.green),
            std::max(blue, _color.min.blue)
        );
    }

}
