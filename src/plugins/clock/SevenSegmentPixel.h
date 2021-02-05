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

#include <avr/pgmspace.h>

#ifndef IOT_CLOCK_LED_PIN
#define IOT_CLOCK_LED_PIN                                           12
#endif

#ifndef IOT_CLOCK_USE_DITHERING
#define IOT_CLOCK_USE_DITHERING                                     1
#endif

#ifndef IOT_CLOCK_USE_FAST_LED_BRIGHTNESS
#define IOT_CLOCK_USE_FAST_LED_BRIGHTNESS                           1
#endif

#if (IOT_CLOCK_USE_DITHERING || IOT_CLOCK_USE_FAST_LED_BRIGHTNESS) && IOT_CLOCK_NEOPIXEL
#error IOT_CLOCK_USE_DITHERING and IOT_CLOCK_USE_FAST_LED_BRIGHTNESS not supported with IOT_CLOCK_NEOPIXEL
#endif


#if !IOT_LED_MATRIX
static constexpr char _digits2SegmentsTable[]  = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71 };  // 0-F
#endif

#if IOT_CLOCK_USE_DITHERING && IOT_CLOCK_USE_FAST_LED_BRIGHTNESS == 0

// use normal brightness and FastLED brightness if dithering is enabled

class SevenSegmentPixelParams {
public:
    static constexpr uint8_t kMaxBrightness = 0xff;

public:
    SevenSegmentPixelParams() : millis(0), fastLEDBrightness(kMaxBrightness / 4), _brightness(kMaxBrightness / 4) {}

    uint8_t brightness() const {
        return _brightness;
    }
    void setBrightness(uint8_t brightness, bool dithering) {
        brightness = dithering ? kMaxBrightness : _brightness;
        fastLEDBrightness = brightness;
    }
public:
    uint32_t millis;
    uint8_t fastLEDBrightness;
private:
    uint8_t _brightness;
};

#elif IOT_CLOCK_USE_FAST_LED_BRIGHTNESS

// use FastLED brightness only

class SevenSegmentPixelParams {
public:
    static constexpr uint8_t kMaxBrightness = 0xff;

public:
    SevenSegmentPixelParams() : millis(0), fastLEDBrightness(kMaxBrightness / 4) {}

    static constexpr uint8_t brightness() {
        return kMaxBrightness;
    }

    void setBrightness(uint8_t brightness, bool dithering) {
        fastLEDBrightness = brightness;
    }

public:
    uint32_t millis;
    uint8_t fastLEDBrightness;
};

#else

// use normal brightness only

class SevenSegmentPixelParams {
public:
    static constexpr uint8_t kMaxBrightness = 0xff;

public:
    SevenSegmentPixelParams() : millis(0), _brightness(kMaxBrightness / 4) {}

    uint8_t brightness() const {
        return _brightness;
    }
    void setBrightness(uint8_t brightness, bool dithering) {
        brightness = brightness;
    }
public:
    uint32_t millis;
private:
    uint8_t _brightness;
};

#endif

template<typename PIXEL_TYPE, size_t NUM_DIGITS, size_t NUM_DIGIT_PIXELS, size_t NUM_COLONS, size_t NUM_COLON_PIXELS>
class SevenSegmentPixel {
public:
    using Params_t = SevenSegmentPixelParams;
    using PixelAddressType = PIXEL_TYPE;
    using ColorType = uint32_t;
    using FadingRefreshCallback = std::function<void(uint8_t brightness)>;
    using FadingFinishedCallback = FadingRefreshCallback;

    using GetColorCallback = std::function<ColorType(PixelAddressType addr, ColorType color, const Params_t &params)>;
    using AnimationCallback = GetColorCallback;

#if IOT_LED_MATRIX
    typedef enum {
        NUM = 1,
    } SegmentEnum_t;
#else
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
#endif

    typedef enum {
        NONE = 0,
        LOWER = 1,
        UPPER = 2,
        BOTH = LOWER|UPPER,
    } ColonEnum_t;

    static constexpr uint8_t kMaxBrightness = Params_t::kMaxBrightness;
    static constexpr size_t kTotalPixelCount = (NUM_DIGITS * NUM_DIGIT_PIXELS * SevenSegmentPixel::SegmentEnum_t::NUM) + (NUM_COLONS * NUM_COLON_PIXELS * 2);

    static_assert(kTotalPixelCount < 512, "max. number of pixel exceeded");

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

#if !IOT_LED_MATRIX
    static constexpr uint8_t getSegmentNumberBits(uint8_t number) {
        return (uint8_t)_digits2SegmentsTable[number & 0xf];
    }
#endif

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
        _dithering(false),
        _canRefresh(false)
    {
#if IOT_CLOCK_NEOPIXEL
        _pixels.begin();
#endif
        reset();
    }

    inline void reset() {
        _pixels.fill(0);
        show();
    }

#if IOT_LED_MATRIX

    inline void clear() {
        for (uint16_t i = IOT_LED_MATRIX_START_ADDR; i < IOT_LED_MATRIX_START_ADDR + kTotalPixelCount; i++) {
            setPixel(i, 0);
        }
    }

    void setColor(ColorType color) {
        _pixels.fill(color);
    }

    void setPixels(ColorType color) {
        for(int16_t i = 0; i < kTotalPixelCount; i++) {
            setPixel(i, color);
        }
    }

    ColorType getPixel(int16_t offset) {
        auto px = _pixels.at(offset + IOT_LED_MATRIX_START_ADDR);
        return (px.red<<16)|(px.green<<8)|(px.blue);
    }

    void __setPixel(int16_t offset, ColorType color) {
#if DEBUG
        if (offset >= 0 && offset < _pixels.size()) {
            _pixels[offset]  = color;
        }
        else {
            __DBG_printf("address out of bounds: %u", offset);
        }

#else
        _pixels[offset]  = color;
#endif
    }

    void _setPixel(int16_t offset, ColorType color) {
        if (color) {
            color = _getColorAlways(offset, color, _params);
        }
        __setPixel(offset, color);
    }

    void setPixel(uint16_t addr, ColorType color) {
        _setPixel(addr + IOT_LED_MATRIX_START_ADDR, color);
    }

    void print(const String &text, ColorType color) {
    }

    void print(const char *text, ColorType color) {
    }

    void dump(Print &output) {
        output.printf_P(PSTR("total pixels: %u\n"), getTotalPixels());
#if IOT_LED_MATRIX_START_ADDR
        output.printf_P(PSTR("start address: %u\n"), IOT_LED_MATRIX_START_ADDR);
#endif
    }

#else

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

    inline void clear() {
        _pixels.fill(0);
    }

    inline void setPixels(ColorType color) {
        _pixels.fill(color);
    }

    inline void fill(ColorType color) {
        setPixels(color);
    }

    inline void setColor(ColorType color) {
        setPixels(color);
    }

    inline void setPixel(uint16_t addr, ColorType color) {
        _pixels.operatror[addr]  = color;
    }

    inline void setPixelByAddr(uint16_t addr, ColorType color) {
        setPixel(addr, color);
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

#endif

    // millis is passed to AnimationCallback
    // the animation should produce the same result for the same value of millis

    void setMillis(uint32_t millis) {
        _params.millis = millis;
        srand(millis);
    }

private:
    void _applyBrightness() {
        #if IOT_CLOCK_USE_FAST_LED_BRIGHTNESS
            _canRefresh = true;
            FastLED.setBrightness(_params.fastLEDBrightness);
        #elif IOT_CLOCK_USE_DITHERING
            if (_dithering) {
                _canRefresh = true;
                FastLED.setBrightness(_params.fastLEDBrightness);
            }
            else {
                _canRefresh = false;
                FastLED.setBrightness(255);
            }
        #endif
    }

public:
    void setBrightness(uint8_t brightness) {
        _params.setBrightness(brightness, _dithering);
        _applyBrightness();
    }

    void setParams(uint32_t millis, uint8_t brightness) {
        _params.millis = millis;
        srand(millis);
        setBrightness(brightness);
    }

    void setDithering(bool dithering) {
#if IOT_CLOCK_USE_DITHERING
        clear();
        _dithering = dithering;
        _params.setBrightness(_params.fastLEDBrightness, _dithering);
        if (_dithering) {
            FastLED.setDither(BINARY_DITHER);
        }
        else {
            FastLED.setDither(DISABLE_DITHER);
        }
        _applyBrightness();
#endif
    }

    inline void show() {
        #if IOT_CLOCK_NEOPIXEL
            _pixels.show();
        #else
            _applyBrightness();
            FastLED.show();
        #endif
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
                return _adjustBrightnessAlways(color, params.brightness());
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
            return _adjustBrightnessAlways(color, params.brightness());
        }
    }

public:

    static inline ColorType _adjustBrightness(ColorType color, uint8_t brightness) {
        if (color) {
            return _adjustBrightnessAlways(color, brightness);
        }
        return color;
    }

    static inline ColorType _adjustBrightnessAlways(ColorType color, uint16_t brightness) {
        auto ptr = reinterpret_cast<uint8_t *>(&color);
        if (*ptr) {
            *ptr = *ptr * brightness / kMaxBrightness;
        }
            ptr++;
        if (*ptr) {
            *ptr = *ptr * brightness / kMaxBrightness;
        }
        ptr++;
        if (*ptr) {
            *ptr = *ptr * brightness / kMaxBrightness;
        }
        return color;
    }

    // returns true if settings like gamma correction, brightness, dithering etc.. can be changed without redrawing the animation
    bool canRefreshWithoutRedraw() const {
        return _canRefresh;
    }

private:
    friend class ClockPlugin;
    friend class BrightnessTimer;

#if IOT_CLOCK_NEOPIXEL
    Adafruit_NeoPixel _pixels;
    #error TODO add [] operator to Adafruit_NeoPixel
#else
    using PixelArray = std::array<CRGB, getTotalPixels()>;

    friend class ClockPlugin;

    PixelArray _pixels;
    CLEDController &_controller;
#endif

#if !IOT_LED_MATRIX
    // std::array<PixelAddressType, getDigitsPixels()> _pixelAddress;
    std::array<PixelAddressType, getColonsPixels()> _colonPixelAddress;
    // std::array<PixelAddressType, getTotalPixels()> _pixelOrder;

    using PixelAddressArray = std::array<PixelAddressType, NUM_DIGIT_PIXELS>;
    using SegmentArray = std::array<PixelAddressArray, SevenSegmentPixel::SegmentEnum_t::NUM>;
    using DigitsArray = std::array<SegmentArray, NUM_DIGIT_PIXELS>;

    DigitsArray _pixelAddress;
#endif
    Params_t _params;
    bool _dithering;
    bool _canRefresh;
};
