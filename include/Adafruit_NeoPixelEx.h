/**
 * Author: sascha_lammers@gmx.de
 */

//
// this class allows to use shared data rather than allocating memory for each strip
// due to the lack of real time brightness scaling, the stack (ESP8266) or heap is used to
// create a copy and scale the data before displaying it
//

#pragma once

#include <Arduino_compat.h>
#include <Adafruit_NeoPixel.h>
#include <alloca.h>

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

    void updateLength(uint16_t n, uint8_t *data);
    void setBrightness(uint8_t brightness);
    void show();

private:
    // do not allow this method to be called
    void updateLength(uint16_t n) {}

private:
    #if ESP8266
        static constexpr size_t kMaxStackAllocation = 384; // use the stack for up to 384 bytes (128 pixels)
    #else
        static constexpr size_t kMaxStackAllocation = 0;
    #endif

    int _brightness;
};

inline Adafruit_NeoPixelEx::Adafruit_NeoPixelEx(uint16_t n, uint8_t *data, int16_t pin, neoPixelType type) :
    Adafruit_NeoPixel(1, pin, type),
    _brightness(Adafruit_NeoPixel::getBrightness())
{
    free(pixels); // NeoPixel calls free on null pointers, so we don't care
    updateLength(n, data);
}

inline Adafruit_NeoPixelEx::~Adafruit_NeoPixelEx()
{
    pixels = nullptr; // see ctor
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
    if (n && data) {
        pixels = data;
        numBytes = n * ((wOffset == rOffset) ? 3 : 4);
        numLEDs = n;
    }
    else {
        pixels = nullptr;
        numBytes = 0;
        numLEDs = 0;
    }
    __LDBG_printf("pin=%u pixels=%p num=%u bytesPerPixel=%u", pin, pixels, numLEDs, ((wOffset == rOffset) ? 3 : 4));
    if (pixels) {
        __LDBG_printf("begin pin=%d", pin);
        Adafruit_NeoPixel::begin();
    }
}

inline void Adafruit_NeoPixelEx::setBrightness(uint8_t brightness)
{
    // store brightness in our own variable, since NeoPixel uses different values in different versions
    if (brightness != _brightness) {
        _brightness = brightness;
        __LDBG_printf("brightness=%d pin=%u", _brightness, pin);
    }
}

inline void Adafruit_NeoPixelEx::show()
{
    if (!pixels) {
        return;
    }
    if (_brightness < 255) {
        // we need to reduce the brightness
        // since NeoPixel cannot do it on the fly, we have to create a copy of the data
        uint8_t *tmp;
        if __CONSTEXPR17 (kMaxStackAllocation) {
            if (numBytes <= kMaxStackAllocation) {
                tmp = static_cast<uint8_t *>(alloca(numBytes));
            }
            else {
                tmp = static_cast<uint8_t *>(malloc(numBytes));
            }
        }
        else {
            tmp = static_cast<uint8_t *>(malloc(numBytes));
        }
        if (!tmp) {
            __DBG_printf("out of memory size=%u (max_alloca=%u)", numBytes, kMaxStackAllocation);
            return;
        }
        memmove(tmp, pixels, numBytes);
        std::swap(tmp, pixels);
        brightness = 0; // force set brightness to scale every time
        Adafruit_NeoPixel::setBrightness(_brightness);
        Adafruit_NeoPixel::show();
        std::swap(tmp, pixels);
        if __CONSTEXPR17 (kMaxStackAllocation) {
            if (numBytes > kMaxStackAllocation) {
                free(tmp);
            }
        }
        else {
            free(tmp);
        }
        return;
    }
    // show full brightness
    Adafruit_NeoPixel::show();
}

#if DEBUG_ADAFRUIT_NEOPIXEL_EX
#    include <debug_helper_disable.h>
#endif
