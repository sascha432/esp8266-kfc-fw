/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_CLOCK_NEOPIXEL

#include <Adafruit_NeoPixel.h>

#else

#if defined(ESP8266)
#ifndef FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_RAW_PIN_ORDER                           1
#endif
#endif
// #ifndef FASTLED_ALLOW_INTERRUPTS
// #define FASTLED_ALLOW_INTERRUPTS                                0
// #endif
// #ifndef FASTLED_INTERRUPT_RETRY_COUNT
// #define FASTLED_INTERRUPT_RETRY_COUNT                           1
// #endif
#define FASTLED_INTERNAL
#include <FastLED.h>

#endif

#include <EventScheduler.h>
#include <avr/pgmspace.h>

#ifndef IOT_CLOCK_LED_PIN
#define IOT_CLOCK_LED_PIN                                           12
#endif

#ifndef IOT_CLOCK_USE_FAST_LED_BRIGHTNESS
#define IOT_CLOCK_USE_FAST_LED_BRIGHTNESS                           0
#endif

static constexpr char _digits2SegmentsTable[]  = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71 };  // 0-F

#include <OSTimer.h>

template<typename PIXEL_TYPE, size_t NUM_DIGITS, size_t NUM_DIGIT_PIXELS, size_t NUM_COLONS, size_t NUM_COLON_PIXELS>
class SevenSegmentPixel {
public:
    using PixelAddressType = PIXEL_TYPE;
    using ColorType = uint32_t;
    using BrightnessType = uint16_t;
    using FadingRefreshCallback = std::function<void(BrightnessType brightness)>;
    using FadingFinishedCallback = FadingRefreshCallback;

    typedef struct {
        uint32_t millis;
        BrightnessType brightness;
    } Params_t;

    using GetColorCallback = std::function<ColorType(PixelAddressType addr, ColorType color, const Params_t &params)>;
    using AnimationCallback = GetColorCallback;

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

    static constexpr BrightnessType kMaxBrightness = std::numeric_limits<BrightnessType>::max();
    static constexpr size_t kTotalPixelCount = (NUM_DIGITS * NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM) + (NUM_COLONS * NUM_COLON_PIXELS * 2);

    static constexpr size_t getNumDigits() {
        return NUM_DIGITS;
    }

    static constexpr size_t getNumPixelsPerDigit() {
        return NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM;
    }

    static constexpr size_t getTotalPixels() {
        return kTotalPixelCount;
    }

    static constexpr size_t getDigitsPixels() {
        return (NUM_DIGITS * NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM);
    }

    static constexpr size_t getColonsPixels() {
        return (NUM_COLONS * NUM_COLON_PIXELS * 2);
    }

    static constexpr uint8_t getSegmentNumberBits(uint8_t number) {
        return (uint8_t)_digits2SegmentsTable[number & 0xf];
    }

    // static constexpr ColorType getSegmentColor(uint32_t color, uint8_t segment, uint8_t number) {
    //     return (_digits2SegmentsTable[number & 0xf] & (1 << segment)) ? color : 0;
    // }

    // static constexpr PixelAddressType getPixelAddress(uint8_t digit, uint8_t pixel, uint8_t segment) {
    //     return (digit * NUM_DIGIT_PIXELS * SegmentEnum_t::NUM) + (segment * NUM_DIGIT_PIXELS) + pixel;
    // }

public:

    SevenSegmentPixel() :
#if IOT_CLOCK_NEOPIXEL
        _pixels(getTotalPixels(), IOT_CLOCK_LED_PIN, NEO_GRB|NEO_KHZ800),
#else
        _controller( FastLED.addLeds<IOT_CLOCK_FASTLED_CHIPSET, IOT_CLOCK_LED_PIN>(_pixels.data(), _pixels.size()) ),
#endif
        _targetBrightness(0),
        _params({0, kMaxBrightness / 3})
    {
#if IOT_CLOCK_NEOPIXEL
        _pixels.begin();
#endif
    }

    ~SevenSegmentPixel() {
        _brightnessTimer.remove();
    }

    PixelAddressType setSegments(uint8_t digit, PixelAddressType offset, PGM_P order) {
        if (digit < NUM_DIGITS) {
            auto &tmp = _pixelAddress[digit];
            for(size_t i = 0; i < NUM_DIGIT_PIXELS; i++) {
                auto ptr = order;
                for(size_t j = 0; j < SegmentEnum_t::NUM; j++) {
                    tmp[j][i] = offset + i + (NUM_DIGIT_PIXELS * pgm_read_byte(ptr++));
                }
            }
        }
        return offset + (SegmentEnum_t::NUM * NUM_DIGIT_PIXELS);
    }

    PixelAddressType setColons(uint8_t num, PixelAddressType lowerAddress, PixelAddressType upperAddress) {
        if (num < NUM_COLONS) {
            _colonPixelAddress[num++] = lowerAddress;
            _colonPixelAddress[num++] = upperAddress;
        }
        return num;
    }

    // set millis for the AnimationCallback
    // can be any value
    inline void setMillis(uint32_t millis) {
        _params.millis = millis;
    }

    inline void show() {
#if IOT_CLOCK_NEOPIXEL
        _pixels.show();
#else
    #if IOT_CLOCK_USE_FAST_LED_BRIGHTNESS
        FastLED.setBrightness(_params.brightness >> 8);
    #endif
        FastLED.show();
#endif
    }

    inline void clear() {
        _pixels.fill(0);
    }

    inline void setColor(ColorType color) {
        _pixels.fill(color);
        show();
    }

    void clearDigit(uint8_t digit) {
        for(const auto &segments: _pixelAddress[digit]) {
            for(const auto addr: segments) {
                _pixels[addr]  = 0;
            }
        }
    }

    void setDigit(uint8_t digit, uint8_t number, ColorType color) {
        if (color == 0) {
            clearDigit(digit);
        }
        else {
            uint8_t numberBits = getSegmentNumberBits(number);
            for(const auto &segments: _pixelAddress[digit]) {
                if (numberBits & 0x01) {
                    for(const auto addr: segments) {
                        _pixels[addr]  = _getColorAlways(addr, color, _params);
                    }
                }
                else {
                    for(const auto addr: segments) {
                        _pixels[addr]  = 0;
                    }
                }
                numberBits >>= 1;
            }
        }
    }

    void setColon(uint8_t num, ColonEnum_t type, ColorType color) {
        PixelAddressType addr;
        if (type & ColonEnum_t::LOWER) {
            addr = _colonPixelAddress[num];
            for(uint8_t i = 0; i < NUM_COLON_PIXELS; i++) {
                _pixels[addr] = _getColor(addr, color, _params);
                addr++;
            }
        }
        if (type & ColonEnum_t::UPPER) {
            addr = _colonPixelAddress[num + 1];
            for(uint8_t i = 0; i < NUM_COLON_PIXELS; i++) {
                _pixels[addr] = _getColor(addr, color, _params);
                addr++;
            }
        }
    }

    void clearColon(uint8_t num) {
        setColon(num, BOTH, 0);
    }

    // void rotate(uint8_t digit, uint8_t position, ColorType color, PixelAddressType *order, size_t orderSize) {
    //     clearDigit(digit);
    //     auto addr = order ?
    //         order[digit * orderSize + position] :
    //         _pixelAddress2[digit][position / IOT_CLOCK_NUM_PIXELS][position % IOT_CLOCK_NUM_PIXELS]
    //         /*_pixelAddress[getPixelAddress(digit, position % IOT_CLOCK_NUM_PIXELS, position / IOT_CLOCK_NUM_PIXELS)]*/
    //     ;
    //     setPixelColor(addr, _getColor(addr, color, _params));
    // }

    // void setSegment(uint8_t digit, SegmentEnum_t segment, ColorType color) {
    //     setSegment(digit, (int)segment, color);
    // }

    // void setSegment(uint8_t digit, int segment, ColorType color) {
    //     segment = ((uint8_t)segment) % SegmentEnum_t::NUM;
    //     for(uint8_t i = 0; i < NUM_DIGIT_PIXELS; i++) {
    //         auto addr = getPixelAddress(digit, i, segment);
    //         setPixelColor(addr, _getColor(addr, color, _params));
    //     }
    // }

    /**
     * Format: 12:34.56
     *
     * Digits: 0-9, # = clear digit
     * Colons: . : or space = no colon
     *
     * "12:00"="12:00", "#1.1#""=" 1.1 ", "## 00"="   00", ...
     *
     * */
    void print(const String &text, ColorType color) {
        print(text.c_str(), color);
    }

    void print(const char *text, ColorType color) {
        if (!text || !*text) {
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

    void setBrightness(BrightnessType brightness) {
        _params.brightness = brightness;
    }

    void setBrightness(BrightnessType brightness, float fadeTime, FadingFinishedCallback finishedCallback = nullptr, FadingRefreshCallback refreshCallback = nullptr) {
        _targetBrightness = brightness;
        if (!_brightnessTimer) {
            auto steps = (BrightnessType)(kMaxBrightness / (fadeTime * (1000 / 20.0))); // 20ms/50Hz refresh rate
            if (!steps) {
                steps = 1;
            }
            //__LDBG_printf("to=%u steps=%u time=%f", _brightness, steps, fadeTime);
            _Timer(_brightnessTimer).add(20, true, [this, steps, finishedCallback, refreshCallback](Event::CallbackTimerPtr timer) {
                int32_t tmp = _params.brightness;
                if (tmp < _targetBrightness) {
                    tmp += steps;
                    if (tmp > _targetBrightness) {
                        tmp = _targetBrightness;
                    }
                    _params.brightness = tmp;
                }
                else if (tmp > _targetBrightness) {
                    tmp -= steps;
                    if (tmp < _targetBrightness) {
                        tmp = _targetBrightness;
                    }
                    _params.brightness = tmp;
                }
                else {
                    timer->disarm();
                    if (finishedCallback) {
                        finishedCallback(_params.brightness);
                    }
                    return;
                }
                if (refreshCallback) {
                    refreshCallback(_params.brightness);
                }

            }, Event::PriorityType::HIGHEST);
        }
    }

    char getSegmentChar(int segment) {
        return 'a' + (segment % SegmentEnum_t::NUM);
    }

    char getSegmentChar(SegmentEnum_t segment) {
        return getSegmentChar((int)segment);
    }

    // dump pixel addresses
    void dump(Print &output) {
        for(size_t d = 0; d < NUM_DIGITS; d++) {
            for(uint8_t s = 0; s < SegmentEnum_t::NUM; s++) {
                output.printf_P(PSTR("segment: digit=%u, segment=%c, addresses="), d, getSegmentChar(s));
                for(size_t p = 0; p < NUM_DIGIT_PIXELS; p++) {
                    output.print(_pixelAddress[d][s][p]);
                    // output.print(_pixelAddress[getPixelAddress(d, p, s)]);
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
    void setCallback(AnimationCallback callback) {
        _callback = callback;
    }
    bool hasCallback() const {
        return (bool)_callback;
    }
    AnimationCallback &getCallback() {
        return _callback;
    }

private:

    GetColorCallback _callback;

    inline ColorType _getColor(PixelAddressType addr, ColorType color, const Params_t &params)
    {
        if (color) {
            if (_callback) {
                return _callback(addr, color, params);
            }
            else {
                return _adjustBrightnessAlways(color, params.brightness);
            }
        }
        return color;
    }

    inline ColorType _getColorAlways(PixelAddressType addr, ColorType color, const Params_t &params)
    {
        if (_callback) {
            return _callback(addr, color, params);
        }
        else {
            return _adjustBrightnessAlways(color, params.brightness);
        }
    }

public:

    static inline ColorType _adjustBrightness(ColorType color, BrightnessType brightness) {
        if (color) {
            return _adjustBrightnessAlways(color, brightness);
        }
        return color;
    }

    static inline ColorType _adjustBrightnessAlways(ColorType color, BrightnessType brightness) {
        auto ptr = reinterpret_cast<uint8_t *>(&color);
        *ptr = *ptr * brightness / kMaxBrightness; ptr++;
        *ptr = *ptr * brightness / kMaxBrightness; ptr++;
        *ptr = *ptr * brightness / kMaxBrightness;
        return color;
    }

private:
    friend class ClockPlugin;
    friend class BrightnessTimer;

#if IOT_CLOCK_NEOPIXEL
    Adafruit_NeoPixel _pixels;
    #error TODO add [] operator to Adafruit_NeoPixel
#else
    std::array<CRGB, getTotalPixels()> _pixels;
    CLEDController &_controller;
#endif
    // std::array<PixelAddressType, getDigitsPixels()> _pixelAddress;
    std::array<PixelAddressType, getColonsPixels()> _colonPixelAddress;
    // std::array<PixelAddressType, getTotalPixels()> _pixelOrder;

    using PixelAddressArray = std::array<PixelAddressType, NUM_DIGIT_PIXELS>;
    using SegmentArray = std::array<PixelAddressArray, SevenSegmentPixel::SegmentEnum_t::NUM>;
    using DigitsArray = std::array<SegmentArray, NUM_DIGIT_PIXELS>;

    DigitsArray _pixelAddress;

    BrightnessType _targetBrightness;
    Event::Timer _brightnessTimer;
    Params_t _params;
};
