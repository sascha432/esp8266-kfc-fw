/**
 * Author: sascha_lammers@gmx.de
 */

#include "animation_plasma.h"

/*

import math

table = {}
for i in range(0, 256):
    v = (((math.sin(2.0 * math.pi * i / 255.0) + 1.0) / 2.0) * 255.0) - 128.0
    table[i + 1] = v

nl_indent = '\n    '
print("static const int8_t PROGMEM sineTable[%u] = {" % len(table), end=nl_indent)
for n, i in table.items():
    print('%d%s' % (i, (n < len(table) and ', ' or '')), end=(n == len(table)) and '\n};' or (n % 8 == 0) and nl_indent or '')

*/

static const int8_t PROGMEM sineTable[256] = {
    0, 3, 6, 9, 13, 16, 19, 22,
    25, 28, 31, 34, 37, 40, 43, 46,
    49, 52, 55, 58, 60, 63, 66, 68,
    71, 74, 76, 79, 81, 84, 86, 88,
    90, 93, 95, 97, 99, 101, 103, 105,
    106, 108, 110, 111, 113, 114, 115, 117,
    118, 119, 120, 121, 122, 123, 124, 125,
    125, 126, 126, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 126, 126, 125,
    125, 124, 123, 123, 122, 121, 120, 119,
    117, 116, 115, 113, 112, 110, 109, 107,
    105, 104, 102, 100, 98, 96, 94, 92,
    89, 87, 85, 82, 80, 77, 75, 72,
    70, 67, 64, 62, 59, 56, 53, 50,
    48, 45, 42, 39, 36, 33, 30, 27,
    23, 20, 17, 14, 11, 8, 5, 2,
    -2, -5, -8, -11, -14, -17, -20, -23,
    -27, -30, -33, -36, -39, -42, -45, -48,
    -50, -53, -56, -59, -62, -64, -67, -70,
    -72, -75, -77, -80, -82, -85, -87, -89,
    -92, -94, -96, -98, -100, -102, -104, -105,
    -107, -109, -110, -112, -113, -115, -116, -117,
    -119, -120, -121, -122, -123, -123, -124, -125,
    -125, -126, -126, -127, -127, -127, -127, -127,
    -127, -127, -127, -127, -127, -126, -126, -125,
    -125, -124, -123, -122, -121, -120, -119, -118,
    -117, -115, -114, -113, -111, -110, -108, -106,
    -105, -103, -101, -99, -97, -95, -93, -90,
    -88, -86, -84, -81, -79, -76, -74, -71,
    -68, -66, -63, -60, -58, -55, -52, -49,
    -46, -43, -40, -37, -34, -31, -28, -25,
    -22, -19, -16, -13, -9, -6, -3, 0
};

int8_t Clock::PlasmaAnimation::_readSineTab(uint8_t ofs)
{
     return pgm_read_byte(sineTable + ofs);
}

template<typename _Ta>
void Clock::PlasmaAnimation::_copyTo(_Ta &output, uint32_t millisValue)
{
    float t = millisValue * (_cfg.speed * ((1.0 / (192.0 * 100000.0))));
    float angle1 = _cfg.angle1 * t;
    float angle2 = _cfg.angle2 * t;
    float angle3 = _cfg.angle3 * t;
    float angle4 = _cfg.angle4 * t;
    uint32_t hueShift = _cfg.hue_shift / (millisValue >> 14); // limit hue_shift to 16.384 per second

    float x1, x2, x3, x4, y1, y2, y3, y4, sx1, sx2, sx3, sx4;
    sx1 = cos(angle1) * radius1 + centerX1;
    sx2 = cos(angle2) * radius2 + centerX2;
    sx3 = cos(angle3) * radius3 + centerX3;
    sx4 = cos(angle4) * radius4 + centerX4;
    y1 = sin(angle1) * radius1 + centerY1;
    y2 = sin(angle2) * radius2 + centerY2;
    y3 = sin(angle3) * radius3 + centerY3;
    y4 = sin(angle4) * radius4 + centerY4;

    CHSV color(0, 255, 255);

    for(CoordinateType y = 0; y < output.getRows(); y++) {
        x1 = sx1;
        x2 = sx2;
        x3 = sx3;
        x4 = sx4;
        for(CoordinateType x = 0; x < output.getCols(); x++) {
            uint32_t value = hueShift
                + _readSineTab((x1 * x1 + y1 * y1) / _cfg.x_size)
                + _readSineTab((x2 * x2 + y2 * y2) / _cfg.x_size)
                + _readSineTab((x3 * x3 + y3 * y3) / _cfg.y_size)
                + _readSineTab((x4 * x4 + y4 * y4) / _cfg.y_size);

            color.hue = value / 4;
            output.setPixel(PixelCoordinatesType(y, x), color);

            x1--;
            x2--;
            x3--;
            x4--;
        }
        y1--;
        y2--;
        y3--;
        y4--;
    }
}
