/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Arduino_compat.h"

/*

- support for ESP8266 @ 80 and 160MHz
- support for GPIO16
- option for ESP8266 to use precaching instead of IRAM
- support for brightness scaling
- support for uint8_t and NeoPixelGRB(3 bytes)
- support for interrupts

example:

bright red, green, blue + 7 low intesity red pixels

NeoPixelGRB pixels[10] = { NeoPixelGRB(0xff0000), NeoPixelGRB(0x00ff00), NeoPixelGRB(0x0000ff) };
NeoPixel_fillColor(&pixels[3], 30 - 9, 0x100000);
NeoPixel_espShow(WS2812_OUTPUT_PIN, pixels, sizeof(pixels));

*/

// enable the brightness scaling
#ifndef NEOPIXEL_HAVE_BRIGHTHNESS
#   define NEOPIXEL_HAVE_BRIGHTHNESS 1
#endif

#if defined(ESP8266)
// allow interrupts during the output. recommended for more than a couple pixels
// interrupts that take too long will abort the current frame and increment NeoPixel_getAbortedFrames
#   ifndef NEOPIXEL_ALLOW_INTERRUPTS
#   define NEOPIXEL_ALLOW_INTERRUPTS 1
#   endif
// use preching instead of IRAM
#   ifndef NEOPIXEL_USE_PRECACHING
#       define NEOPIXEL_USE_PRECACHING 1
#   endif
#   if NEOPIXEL_USE_PRECACHING
#       define NEOPIXEL_ESPSHOW_FUNC_ATTR PRECACHE_ATTR
#   else
#       define NEOPIXEL_ESPSHOW_FUNC_ATTR IRAM_ATTR
#   endif
#else
#   define NEOPIXEL_USE_PRECACHING 0
#endif


struct NeoPixelGRB {
    uint8_t g;
    uint8_t r;
    uint8_t b;

    NeoPixelGRB() :
        g(0),
        r(0),
        b(0)
    {
    }

    NeoPixelGRB(uint32_t rgb) :
        g(static_cast<uint8_t>(rgb >> 8)),
        r(static_cast<uint8_t>(rgb >> 16)),
        b(static_cast<uint8_t>(rgb))
    {
    }

    NeoPixelGRB(uint8_t red, uint8_t green, uint8_t blue) :
        g(green),
        r(red),
        b(blue)
    {
    }

    uint32_t toRGB() const {
        return (r << 16) | (g << 8) | b;
    }
};

#ifdef __cplusplus
extern "C" {
#endif

// only available with NEOPIXEL_ALLOW_INTERRUPTS=1
extern uint32_t NeoPixel_getAbortedFrames;

void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color);
void NeoPixel_espShow(uint8_t pin, const uint8_t *pixels, uint16_t numBytes, uint8_t brightness = 255);

#ifdef __cplusplus
}
#endif

inline static void NeoPixel_fillColor(NeoPixelGRB *pixels, uint16_t numBytes, uint32_t color) {
    NeoPixel_fillColor(reinterpret_cast<uint8_t *>(pixels), numBytes, color);
}

inline static void NeoPixel_espShow(uint8_t pin, const NeoPixelGRB *pixels, uint16_t numBytes) {
    NeoPixel_espShow(pin, reinterpret_cast<const uint8_t *>(pixels), numBytes);
}
