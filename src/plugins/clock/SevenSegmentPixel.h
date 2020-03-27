/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_CLOCK_NEOPIXEL
#include <Adafruit_NeoPixel.h>
#else
#include <FastLED.h>
#endif
#include <EventTimer.h>

#ifndef IOT_CLOCK_NEOPIXEL_PIN
#define IOT_CLOCK_NEOPIXEL_PIN                                   12
#endif

#define SevenSegmentPixel_PIXEL_ADDRESS(digit, pixel, segment)   ((digit * _numPixels * SegmentEnum_t::NUM) + (segment * _numPixels) + pixel)
#define SevenSegmentPixel_SEGMENT_TO_BIT(segment)                (1 << segment)
#define SevenSegmentPixel_COLOR(color, segment, bitset)          ((bitset & SevenSegmentPixel_SEGMENT_TO_BIT(segment)) ? color : 0)

#define SevenSegmentPixel_NUM_PIXELS(digits, numPixel, colons)   ((digits * numPixel * SevenSegmentPixel::SegmentEnum_t::NUM) + (colons * (IOT_CLOCK_NUM_PX_PER_DOT * 2) * sizeof(SevenSegmentPixel::pixel_address_t)))

#define SevenSegmentPixel_DIGITS_NUM_PIXELS                      (int)SevenSegmentPixel_NUM_PIXELS(IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_DIGITS, 0)
#define SevenSegmentPixel_TOTAL_NUM_PIXELS                       (int)SevenSegmentPixel_NUM_PIXELS(IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_COLONS)

class SevenSegmentPixel {
public:
    typedef uint8_t pixel_address_t;    // uint16_t for more than 256 pixels
    typedef uint32_t color_t;

    typedef enum {
        A = 0,
        B,
        C,
        D,
        E,
        F,
        G,
        NUM,
    } SegmentEnum_t;

    typedef enum {
        NONE = 0,
        LOWER = 1,
        UPPER = 2,
        BOTH = LOWER|UPPER,
    } ColonEnum_t;

    static const uint16_t MAX_BRIGHTNESS = 0xffff;
    typedef std::function<void(uint16_t brightness)> Callback_t;

    SevenSegmentPixel(uint8_t numDigits, uint8_t numPixels, uint8_t numColons);
    ~SevenSegmentPixel();

    pixel_address_t setSegments(uint8_t digit, pixel_address_t offset, PGM_P order);
    pixel_address_t setColons(uint8_t num, pixel_address_t lowerAddress, pixel_address_t upperAddress);

    inline void show() __attribute__((always_inline)) {
#if IOT_CLOCK_NEOPIXEL
        _pixels->show();
#else
        FastLED.show();
#endif
    }

    void clear();
    void setColor(color_t color);
    void setColor(pixel_address_t num, color_t color);

    void setDigit(uint8_t digit, uint8_t number, color_t color);

    void clearDigit(uint8_t digit) {
        setDigit(digit, 0xff, 0);
    }

    void setColon(uint8_t num, ColonEnum_t type, color_t color);

    void clearColon(uint8_t num) {
        setColon(num, BOTH, 0);
    }

    void rotate(uint8_t digit, uint8_t position, color_t color, char *order, size_t orderSize);
    void setSegment(uint8_t digit, SegmentEnum_t segment, color_t color) {
        setSegment(digit, (int)segment, color);
    }
    void setSegment(uint8_t digit, int segment, color_t color);

    uint32_t getPixelColor(uint16_t n) const {
        return _getPixels().getPixelColor(n);
    }

    void setPixelColor(uint16_t n, uint32_t c) {
        return _getPixels().setPixelColor(n, c);
    }

    /**
     * Format: 12:34.56
     *
     * Digits: 0-9, # = clear digit
     * Colons: . : or space = no colon
     *
     * "12:00"="12:00", "#1.1#""=" 1.1 ", "## 00"="   00", ...
     *
     * */
    void print(const String &text, color_t color) {
        print(text.c_str(), color);
    }
    void print(const char *text, color_t color);

    uint8_t numDigits() const {
        return _numDigits;
    }
    uint8_t numColons() const {
        return _numColons;
    }
    uint8_t numPixels() const {
        return _numPixels;
    }

    uint16_t getTotalPixelCount() const {
#if IOT_CLOCK_NEOPIXEL
        return _pixels->numPixels();
#else
        return FastLED.size();
#endif
    }

    void setBrightness(uint16_t brightness, float fadeTime, Callback_t callback = nullptr);

    char getSegmentChar(int segment) {
        return 'a' + (segment % SegmentEnum_t::NUM);
    }
    inline char getSegmentChar(SegmentEnum_t segment) {
        return getSegmentChar((int)segment);
    }

    // dump pixel addresses
    void dump(Print &output);


public:
    typedef std::function<color_t(pixel_address_t addr, color_t color)> AnimationCallback_t;

    void setCallback(AnimationCallback_t callback) {
        _callback = callback;
    }

private:
    color_t _getColor(pixel_address_t addr, color_t color);
    color_t _adjustBrightness(color_t color) const;

private:
#if IOT_CLOCK_NEOPIXEL
    Adafruit_NeoPixel &_getPixels() const {
        return *_pixels;
    }

    Adafruit_NeoPixel *_pixels;
#else
    CRGB *_pixels;
    CLEDController *_controller;
#endif
    uint8_t _numDigits;
    uint8_t _numPixels;
    uint8_t _numColons;
    pixel_address_t *_pixelAddress;
    pixel_address_t *_dotPixelAddress;
    pixel_address_t *_pixelOrder;
    AnimationCallback_t _callback;
    uint16_t _brightness;
    uint16_t _targetBrightness;
    EventScheduler::Timer _brightnessTimer;
};
