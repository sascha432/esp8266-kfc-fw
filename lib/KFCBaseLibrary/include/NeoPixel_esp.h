/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Arduino_compat.h"

/*

- support for ESP8266 @ 80 and 160MHz
- support for GPIO16
- no IRAM required, using precaching
- support for uint8_t and SingleNeoPixel(3 bytes)

example:

bright red, green, blue + 7 low intesity red pixels

SingleNeoPixel pixels[10] = { SingleNeoPixel(0xff0000), SingleNeoPixel(0x00ff00), SingleNeoPixel(0x0000ff) };
NeoPixel_fillColor(&pixels[3], 30 - 9, 0x100000);
NeoPixel_espShow(WS2812_OUTPUT_PIN, pixels, sizeof(pixels));

*/

#ifndef NEOPIXEL_HAVE_BRIGHTHNESS
#define NEOPIXEL_HAVE_BRIGHTHNESS 1
#endif

struct SingleNeoPixel {
    uint8_t g;
    uint8_t r;
    uint8_t b;
    SingleNeoPixel() :
        g(0),
        r(0),
        b(0)
    {
    }

    SingleNeoPixel(uint32_t fromRGB) :
        g(static_cast<uint8_t>(fromRGB >> 8)),
        r(static_cast<uint8_t>(fromRGB >> 16)),
        b(static_cast<uint8_t>(fromRGB))
    {
    }

    uint32_t toRGB() const {
        return (r << 16) | (g << 8) | b;
    }
};

#ifdef __cplusplus
extern "C" {
#endif

void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color);
void NeoPixel_espShow(uint8_t pin, const uint8_t *pixels, uint16_t numBytes, uint8_t brightness = 255);

#ifdef __cplusplus
}
#endif

inline static void NeoPixel_fillColor(SingleNeoPixel *pixels, uint16_t numBytes, uint32_t color) {
    NeoPixel_fillColor(reinterpret_cast<uint8_t *>(pixels), numBytes, color);
}

inline static void NeoPixel_espShow(uint8_t pin, const SingleNeoPixel *pixels, uint16_t numBytes) {
    NeoPixel_espShow(pin, reinterpret_cast<const uint8_t *>(pixels), numBytes);
}
