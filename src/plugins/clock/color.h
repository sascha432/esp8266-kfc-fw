/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "clock_base.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace Clock {

    class Color {
    public:
        // possible pairs
        // (4, 85), (6, 51), (16, 17), (18, 15), (52, 5), (86, 3)
        static constexpr uint8_t kRndMod = 6;
        static constexpr uint8_t kRndMul = 51;
        static constexpr uint8_t kRndAnyAbove = 127;
        static_assert((kRndMod - 1) * kRndMul == 255, "invalid mod or mul");


    public:
        Color();
        Color(uint8_t *values);
        Color(uint8_t red, uint8_t green, uint8_t blue);
        Color(uint32_t value);
        Color(CRGB color);

        static Color fromString(const String &value);
        static Color fromBGR(uint32_t value);

        String toString() const;
        // void printAnsiCode(Print &stream) const;
        // static void printClearScreenAnsiCode(Print &stream, uint8_t code = 2, uint16_t row = 0, uint16_t col = 0);
        // static void printResetAnsiCode(Print &stream);
        String implode(char sep) const;

        Color &operator=(CRGB value);
        Color &operator=(uint32_t value);
        Color &operator=(ClockColor_t value);
        operator bool() const;
        operator int() const;
        operator uint32_t() const;
        operator CRGB() const;
        operator ClockColor_t() const;
        bool operator==(int value) const;
        bool operator!=(int value) const;
        bool operator==(uint32_t value) const;
        bool operator!=(uint32_t value) const;
        bool operator==(const Color &value) const;
        bool operator!=(const Color &value) const;

        // set random color
        uint32_t rnd(uint8_t minValue = kRndAnyAbove);
        // uint32_t rnd(Color factor, uint8_t minValue = kRndAnyAbove);

        uint32_t get() const;
        uint8_t red() const;
        uint8_t green() const;
        uint8_t blue() const;

        uint8_t &red();
        uint8_t &green();
        uint8_t &blue();

    private:
        uint8_t _getRand(uint8_t mod, uint8_t mul, uint8_t factor = 255);

        union __attribute__packed__ {
            struct __attribute__packed__ {
                uint8_t _blue;
                uint8_t _green;
                uint8_t _red;
            };
            uint32_t _value: 24;
        };

    };

    inline Color::Color() : _value(0)
    {
    }

    inline Color::Color(uint8_t values[]) : _blue(values[0]), _green(values[1]), _red(values[2])
    {
    }

    inline Color::Color(uint8_t red, uint8_t green, uint8_t blue) : _blue(blue), _green(green), _red(red)
    {
    }

    inline Color::Color(uint32_t value) : _value(value)
    {
    }

    inline Color::Color(CRGB color) : Color(color.red, color.green, color.blue)
    {
    }

    inline Color Color::fromBGR(uint32_t value)
    {
        return Color(value, value >> 8, value >> 16);
    }

    inline bool Color::operator==(uint32_t value) const
    {
        return _value == value;
    }

    inline bool Color::operator!=(uint32_t value) const
    {
        return _value != value;
    }

    inline bool Color::operator==(int value) const
    {
        return _value == value;
    }

    inline bool Color::operator!=(int value) const
    {
        return _value != value;
    }

    inline bool Color::operator==(const Color &value) const
    {
        return value.get() == get();
    }

    inline bool Color::operator!=(const Color &value) const
    {
        return value.get() != get();
    }

    inline uint32_t Color::get() const
    {
        return _value;
    }

    inline Color &Color::operator=(CRGB value)
    {
        _blue = value.blue;
        _green = value.green;
        _red = value.red;
        return *this;
    }

    inline Color &Color::operator=(uint32_t value)
    {
        _value = value;
        return *this;
    }

    inline Color &Color::operator=(ClockColor_t value)
    {
        _value = value.value;
        return *this;
    }

    inline Color::operator bool() const
    {
        return _value != 0;
    }

    inline Color::operator int() const
    {
        return _value;
    }

    inline Color::operator uint32_t() const
    {
        return _value;
    }

    inline Color::operator CRGB() const
    {
        return CRGB(_red, _green, _blue);
    }

    inline Color::operator ClockColor_t() const
    {
        return ClockColor_t(_value);
    }

    inline uint8_t Color::red() const
    {
        return _red;
    }

    inline uint8_t Color::green() const
    {
        return _green;
    }

    inline uint8_t Color::blue() const
    {
        return _blue;
    }

    inline uint8_t &Color::red()
    {
        return _red;
    }

    inline uint8_t &Color::green()
    {
        return _green;
    }

    inline uint8_t &Color::blue()
    {
        return _blue;
    }

    static_assert(sizeof(Color) == 3, "Invalid size");

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
