/**
 * Author: sascha_lammers@gmx.de
 */

//
// this class allows to use shared data rather than allocating memory for each strip
//

#pragma once

#include <Adafruit_NeoPixel.h>

class Adafruit_NeoPixelEx : public Adafruit_NeoPixel
{
public:
    Adafruit_NeoPixelEx(uint16_t n, uint8_t *data, int16_t pin = 6, neoPixelType type = NEO_GRB + NEO_KHZ800);
    ~Adafruit_NeoPixelEx();

    void updateLength(uint16_t n, uint8_t *data); // this method will modify the pixel data and cause loss of data. to keep accurate colors, the data needs to be reset before increasing brightness
    void setBrightness(uint8_t b);

private:
    // do not allow this method to be called
    void updateLength(uint16_t n) {}
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
    pixels = static_cast<uint8_t *>(malloc(1)); // allocate memory for the original destructor
}

inline void Adafruit_NeoPixelEx::updateLength(uint16_t n, uint8_t *data)
{
    pixels = data;
    numBytes = n * ((wOffset == rOffset) ? 3 : 4);
    numLEDs = n;
    _brightness = -1;
}

inline void Adafruit_NeoPixelEx::setBrightness(uint8_t b)
{
    if (b != _brightness) {
        Adafruit_NeoPixel::setBrightness(b);
        _brightness = b;
    }
}
