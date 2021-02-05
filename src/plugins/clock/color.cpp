/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "animation.h"
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Clock;

Color::Color() : _value(0)
{
}

Color::Color(uint8_t *values) : _blue(values[0]), _green(values[1]), _red(values[2])
{
}

Color::Color(uint8_t red, uint8_t green, uint8_t blue) : _blue(blue), _green(green), _red(red)
{
}

Color::Color(uint32_t value) : _value(value)
{
}

Color Color::fromString(const String &value)
{
    auto ptr = value.c_str();
    while(isspace(*ptr) || *ptr == '#') {
        ptr++;
    }
    return static_cast<uint32_t>(strtol(ptr, nullptr, 16) & 0xffffff);
}

Color Color::fromBGR(uint32_t value)
{
    return Color(value, value >> 8, value >> 16);
}

String Color::toString() const
{
    char buf[8];
    snprintf_P(buf, sizeof(buf), PSTR("#%06X"), _value);
    return buf;
}

// void Color::printAnsiCode(Print &stream) const
// {
//     stream.printf_P(PSTR("\x1b[48;2;%u;%u;%um"), _red, _green, _blue);
// }

// void Color::printClearScreenAnsiCode(Print &stream, uint8_t code, uint16_t row, uint16_t col)
// {
//     stream.printf_P(PSTR("\x1b[%uJ\x1b[%u;%uH"), code, row, col);
// }

// void Color::printResetAnsiCode(Print &stream)
// {
//     stream.print(F("\x1b[0m"));
// }

String Color::implode(char sep) const
{
    char buf[16];
    snprintf_P(buf, sizeof(buf), PSTR("%u%c%u%c%u"), _red, sep, _green, sep, _blue);
    return buf;
}

uint8_t Color::_getRand(uint8_t mod, uint8_t mul, uint8_t factor)
{
    return rand() % mod * mul;
    // uint8_t rnd = rand() % mod;
    // if (factor == 255) {
    //     return rnd * mul;
    // }
    // return (rnd * mul * factor) / 255;
}

uint32_t Color::rnd(uint8_t minValue)
{
    do {
        _red = _getRand(kRndMod, kRndMul);
        _green = _getRand(kRndMod, kRndMul);
        _blue = _getRand(kRndMod, kRndMul);
    } while (!_value && !(_red >= minValue || _green >= minValue || _blue >= minValue));

    return _value;
}

// uint32_t Color::rnd(Color factor, uint8_t minValue)
// {
//     uint8_t minRed = red() * minValue / 255;
//     uint8_t count = 0;
//     do {
//         _red = _getRand(kRndMod, kRndMul, red());
//         if (++count == 0) {
//             _red = minValue;
//         }
//     } while (_red < minRed);
//     count = 0;
//     uint8_t minGreen = green() * minValue / 255;
//     do {
//         _green = _getRand(kRndMod, kRndMul, green());
//         if (++count == 0) {
//             _green = minValue;
//         }
//     } while (_green < minGreen);
//     count = 0;
//     uint8_t minBlue = blue() * minValue / 255;
//     do {
//         _blue = _getRand(kRndMod, kRndMul, blue());
//         if (++count == 0) {
//             _blue = minValue;
//         }
//     } while (_blue < minBlue);

//     return _value;
// }
