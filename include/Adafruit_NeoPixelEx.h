/**
 * Author: sascha_lammers@gmx.de
 */

//
// this class allows to use shared data rather than allocating memory for each strip
//

#pragma once

#include <Arduino_compat.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG_ADAFRUIT_NEOPIXEL_EX 0

#if DEBUG_ADAFRUIT_NEOPIXEL_EX
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class Adafruit_NeoPixelEx : public Adafruit_NeoPixel
{
public:
    Adafruit_NeoPixelEx(uint16_t n, uint8_t *data, int16_t pin = 6, neoPixelType type = NEO_GRB + NEO_KHZ800);
    ~Adafruit_NeoPixelEx();

    void end();

    void updateLength(uint16_t n, uint8_t *data); // this method will modify the pixel data and cause loss of data. to keep accurate colors, the data needs to be reset before increasing brightness
    void setBrightness(uint8_t b);

private:
    // do not allow this method to be called
    void updateLength(uint16_t n) {}

private:
    int _brightness;
};

inline Adafruit_NeoPixelEx::Adafruit_NeoPixelEx(uint16_t n, uint8_t *data, int16_t pin, neoPixelType type) :
    Adafruit_NeoPixel(1, pin, type),
    _brightness(-1)
{
    if (pixels) {
        free(pixels);
    }
    updateLength(n, data);
}

inline Adafruit_NeoPixelEx::~Adafruit_NeoPixelEx()
{
    pixels = nullptr; // the adafruit library is calling free on a nullptr, so just reset it to null
}

inline void Adafruit_NeoPixelEx::end()
{
    if (begun) {
        __LDBG_printf("end pin=%d", pin);
        if (pin >= 0) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW); // keep pin low to avoid feeding the LEDs through the data line
        }
        begun = false;
    }
}

inline void Adafruit_NeoPixelEx::updateLength(uint16_t n, uint8_t *data)
{
    if (begun) {
        end();
    }
    pixels = data;
    numBytes = n * ((wOffset == rOffset) ? 3 : 4);
    numLEDs = n;
    _brightness = -1;
    __LDBG_printf("pin=%u pixels=%p num=%u bytesPerPixel=%u", pin, pixels, numLEDs, ((wOffset == rOffset) ? 3 : 4));
    if (pixels && numLEDs) {
        __LDBG_printf("begin pin=%d", pin);
        Adafruit_NeoPixel::begin();
    }
}

inline void Adafruit_NeoPixelEx::setBrightness(uint8_t b)
{
    if (b != _brightness) {
        Adafruit_NeoPixel::setBrightness(b);
        _brightness = b;
        __LDBG_printf("brightness=%d pin=%u", _brightness, pin);
    }
}

#if DEBUG_ADAFRUIT_NEOPIXEL_EX
#    include <debug_helper_disable.h>
#endif
