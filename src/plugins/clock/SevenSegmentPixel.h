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
#include <avr/pgmspace.h>

#ifndef IOT_CLOCK_LED_PIN
#define IOT_CLOCK_LED_PIN                                       12
#endif

static constexpr char _digits2SegmentsTable[]  = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71 };  // 0-F

template<typename PIXEL_ADDR_T, size_t NUM_DIGITS, size_t NUM_DIGIT_PIXELS, size_t NUM_COLONS, size_t NUM_COLON_PIXELS>
class SevenSegmentPixel {
public:
    typedef PIXEL_ADDR_T pixel_address_t;
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

    static constexpr uint16_t MAX_BRIGHTNESS = 0xffff;
    typedef std::function<void(uint16_t brightness)> Callback_t;

    static constexpr size_t getNumDigits() {
        return NUM_DIGITS;
    }

    static constexpr size_t getNumPixelsPerDigit() {
        return NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM;
    }

    static constexpr size_t getTotalPixels() {
        return (NUM_DIGITS * NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM) + (NUM_COLONS * NUM_COLON_PIXELS * 2);
    }

    static constexpr size_t getDigitsPixels() {
        return (NUM_DIGITS * NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM);
    }

    static constexpr size_t getColonsPixels() {
        return (NUM_COLONS * NUM_COLON_PIXELS * 2);
    }

    static constexpr color_t getSegmentColor(uint32_t color, uint8_t segment, uint8_t number) {
        return (_digits2SegmentsTable[number & 0xf] & (1 << segment)) ? color : 0;
    }

    static constexpr pixel_address_t getPixelAddress(uint8_t digit, uint8_t pixel, uint8_t segment) {
        return (digit * NUM_DIGIT_PIXELS * SegmentEnum_t::NUM) + (segment * NUM_DIGIT_PIXELS) + pixel;
    }

public:

    SevenSegmentPixel() :
#if IOT_CLOCK_NEOPIXEL
        _pixels(getTotalPixels(), IOT_CLOCK_LED_PIN, NEO_GRB|NEO_KHZ800),
#else
        _controller( FastLED.addLeds<IOT_CLOCK_FASTLED_CHIPSET, IOT_CLOCK_LED_PIN>(_pixels.data(), getTotalPixels()) ),
#endif
        _callback(nullptr), _brightness(MAX_BRIGHTNESS)
    {
#if IOT_CLOCK_NEOPIXEL
        _pixels.begin();
#endif
    }

    ~SevenSegmentPixel() {
        _brightnessTimer.remove();
    }

    pixel_address_t setSegments(uint8_t digit, pixel_address_t offset, PGM_P order) {
        if (digit < NUM_DIGITS) {
            for(size_t i = 0; i < NUM_DIGIT_PIXELS; i++) {
                auto ptr = order;
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::A)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::B)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::C)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::D)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::E)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::F)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                _pixelAddress[getPixelAddress(digit, i, SegmentEnum_t::G)] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr));
            }
        }
        return offset + (SegmentEnum_t::NUM * NUM_DIGIT_PIXELS);
    }

    pixel_address_t setColons(uint8_t num, pixel_address_t lowerAddress, pixel_address_t upperAddress) {
        if (num < NUM_COLONS) {
            _colonPixelAddress[num++] = lowerAddress;
            _colonPixelAddress[num++] = upperAddress;
        }
        return num;
    }


    inline void show() {
#if IOT_CLOCK_NEOPIXEL
        _pixels.show();
#else
        FastLED.show();
#endif
    }

    inline void clear() {
        setColor(0);
    }

    void setColor(color_t color) {
        for(pixel_address_t i = 0; i < getTotalPixels(); i++) {
            setPixelColor(i, color);
        }
        show();
    }

    void setColor(pixel_address_t num, color_t color) {
        setPixelColor(num, color);
        show();
    }

    void setDigit(uint8_t digit, uint8_t number, color_t color) {
        for(uint8_t j = SegmentEnum_t::A; j < SegmentEnum_t::NUM; j++) {
            color_t segmentColor = getSegmentColor(color, j, number);
            for(size_t i = 0; i < NUM_DIGIT_PIXELS; i++) {
                pixel_address_t addr = _pixelAddress[getPixelAddress(digit, i, j)];
                setPixelColor(addr, _getColor(addr, segmentColor));
            }
        }
    }

    void clearDigit(uint8_t digit) {
        setDigit(digit, 0xff, 0);
    }

    void setColon(uint8_t num, ColonEnum_t type, color_t color) {
        pixel_address_t addr;
        if (type & ColonEnum_t::LOWER) {
            addr = _colonPixelAddress[num];
            for(uint8_t i = 0; i < NUM_COLON_PIXELS; i++) {
                setPixelColor(addr, _getColor(addr, color));
                addr++;
            }
        }
        if (type & ColonEnum_t::UPPER) {
            addr = _colonPixelAddress[num + 1];
            for(uint8_t i = 0; i < NUM_COLON_PIXELS; i++) {
                setPixelColor(addr, _getColor(addr, color));
                addr++;
            }
        }
    }

    void clearColon(uint8_t num) {
        setColon(num, BOTH, 0);
    }

    void rotate(uint8_t digit, uint8_t position, color_t color, pixel_address_t *order, size_t orderSize) {
        clearDigit(digit);
        auto addr = order ? order[digit * orderSize + position] : _pixelAddress[getPixelAddress(digit, position % IOT_CLOCK_NUM_PIXELS, position / IOT_CLOCK_NUM_PIXELS)];
        setPixelColor(addr, _getColor(addr, color));
    }

    void setSegment(uint8_t digit, SegmentEnum_t segment, color_t color) {
        setSegment(digit, (int)segment, color);
    }

    void setSegment(uint8_t digit, int segment, color_t color) {
        segment = ((uint8_t)segment) % SegmentEnum_t::NUM;
        for(uint8_t i = 0; i < NUM_DIGIT_PIXELS; i++) {
            auto addr = getPixelAddress(digit, i, segment);
            setPixelColor(addr, _getColor(addr, color));
        }
    }

    inline color_t getPixelColor(uint16_t n) const {
#if IOT_CLOCK_NEOPIXEL
        return _pixels.getPixelColor(n);
#else
        return _pixels[n];
#endif
    }

    inline void setPixelColor(uint16_t n, uint32_t c) {
#if IOT_CLOCK_NEOPIXEL
        _pixels.setPixelColor(n, c);
#else
        _pixels[n] = c;
#endif
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

    void print(const char *text, color_t color) {
        if (!text || !*text){
            clear();
        }
        uint8_t digit = 0;
        uint8_t colon = 0;
        while(*text) {
            if (isdigit(*text) || *text == '#') {
                if (digit >= NUM_DIGITS) {
                    break;
                }
                if (*text == '#') {
                    clearDigit(digit);
                }
                else {
                    setDigit(digit, *text - '0', color);
                }
                digit++;
            }
            else if (*text == ' ' || *text == '.' || *text == ':') {
                if (colon < NUM_COLONS) {
                    if (*text == ' ') {
                        clearColon(colon);
                    }
                    else if (*text == ':') {
                        setColon(colon, BOTH, color);
                    }
                    else if (*text == '.') {
                        setColon(colon, LOWER, color);
                        setColon(colon, UPPER, 0);
                    }
                    colon++;
                }

            }
            text++;
        }
    }

    void setBrightness(uint16_t brightness) {
        _brightness = brightness;
    }

    void setBrightness(uint16_t brightness, float fadeTime, Callback_t callback = nullptr, Callback_t refreshCallback = nullptr) {
        _targetBrightness = brightness;
        if (!_brightnessTimer.active()) {
            uint16_t steps = (uint16_t)(MAX_BRIGHTNESS / (fadeTime * (1000 / 25.0)));      // 0-100%, 3 seconds fade time
            if (!steps) {
                steps = 1;
            }
            _debug_printf_P(PSTR("to=%u steps=%u time=%f\n"), _brightness, steps, fadeTime);
            _brightnessTimer.add(25, true, [this, steps, callback, refreshCallback](EventScheduler::TimerPtr timer) {
                int32_t tmp = _brightness;
                if (tmp < _targetBrightness) {
                    tmp += steps;
                    if (tmp > _targetBrightness) {
                        tmp = _targetBrightness;
                    }
                    _brightness = tmp;
                }
                else if (tmp > _targetBrightness) {
                    tmp -= steps;
                    if (tmp < _targetBrightness) {
                        tmp = _targetBrightness;
                    }
                    _brightness = tmp;
                }
                else {
                    timer->detach();
                    if (callback) {
                        callback(_brightness);
                    }
                    return;
                }
                if (refreshCallback) {
                    refreshCallback(_brightness);
                }

            }, EventScheduler::PRIO_HIGH);
        }
    }

    char getSegmentChar(int segment) {
        return 'a' + (segment % SegmentEnum_t::NUM);
    }

    inline char getSegmentChar(SegmentEnum_t segment) {
        return getSegmentChar((int)segment);
    }

    // dump pixel addresses
    void dump(Print &output) {
        for(size_t d = 0; d < NUM_DIGITS; d++) {
            for(uint8_t s = 0; s < SegmentEnum_t::NUM; s++) {
                output.printf_P(PSTR("segment: digit=%u, segment=%c, addresses="), d, getSegmentChar(s));
                for(size_t p = 0; p < NUM_DIGIT_PIXELS; p++) {
                    output.print(_pixelAddress[getPixelAddress(d, p, s)]);
                    if (p != NUM_DIGIT_PIXELS - 1) {
                        output.print(F(", "));
                    }
                }
                output.println();
            }
        }
        for(size_t c = 0; c < NUM_COLONS; c++) {
            output.printf_P(PSTR("colon %u: lower address="), c);
            for(size_t p = 0; p < IOT_CLOCK_NUM_COLON_PIXELS; p++) {
                output.printf_P(PSTR("%u "), _colonPixelAddress[c * 2] + p);
            }
            output.println();
            output.printf_P(PSTR("colon %u: upper address="), c);
            for(size_t p = 0; p < IOT_CLOCK_NUM_COLON_PIXELS; p++) {
                output.printf_P(PSTR("%u "), _colonPixelAddress[c * 2 + 1] + p);
            }
            output.println();
        }
        output.printf_P(PSTR("total pixels: %u\n"), getTotalPixels());
        output.printf_P(PSTR("digit pixels: %u\n"), getDigitsPixels());
        output.printf_P(PSTR("colon pixels: %u\n"), getColonsPixels());
    }


public:
    typedef std::function<color_t(pixel_address_t addr, color_t color)> AnimationCallback_t;

    void setCallback(AnimationCallback_t callback) {
        _callback = callback;
    }
    bool hasCallback() const {
        return (bool)_callback;
    }

private:
    color_t _getColor(pixel_address_t addr, color_t color) const {
        if (!color) {
            return 0;
        }
        if (_callback) {
            // if (_pixelOrder) {
            //     return _adjustBrightness(_callback(_pixelOrder[addr], color));
            // }
            return _adjustBrightness(_callback(addr, color));
        }
        return _adjustBrightness(color);
    }

    color_t _adjustBrightness(color_t color) const {
        return (
            ((color & 0xff) * _brightness / MAX_BRIGHTNESS) |
            ((((color >> 8) & 0xff) * _brightness / MAX_BRIGHTNESS) << 8) |
            ((((color >> 16) & 0xff) * _brightness / MAX_BRIGHTNESS) << 16)
        );
    }

private:
#if IOT_CLOCK_NEOPIXEL
    Adafruit_NeoPixel _pixels;
#else
    std::array<CRGB, getTotalPixels()> _pixels;
    CLEDController &_controller;
#endif
    std::array<pixel_address_t, getDigitsPixels()> _pixelAddress;
    std::array<pixel_address_t, getColonsPixels()> _colonPixelAddress;
    // std::array<pixel_address_t, getTotalPixels()> _pixelOrder;

    AnimationCallback_t _callback;
    uint16_t _brightness;
    uint16_t _targetBrightness;
    EventScheduler::Timer _brightnessTimer;
};
