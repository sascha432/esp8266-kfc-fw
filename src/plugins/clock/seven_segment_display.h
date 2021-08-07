/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX == 0

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <bitset>
#include <NeoPixelEx.h>
#include "pixel_display.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#pragma GCC push_options
#pragma GCC optimize ("O3")

// class for 7 segment display with various shapes that can be created with
// the ./scripts/tools/create_7segment_display.py tool
//
// currently up to 6 digits and 2 colons are supported
// the mapping is stored in PROGMEM to reduce memory overhead, the additonal memory required is (total pixels / 8) bytes (32bit padded)
// see kSevenSegmentExtraMemorySize and kSevenSegmentTotalMemorySize (that is including the base class)
//
// high level functions to display the digits and colons or single dots are available
// low level functions for animations are also supported by the base class
//
// the code is optimized for performance and can easily display 2000Hz depending on the type and number of pixels
// for example WS2813 with 800kbit have a refresh rate of 33333Hz per LED, or 333Hz for 100
//
// with a single core MCU like the ESP8266 and many LEDs, disabling interrupts when calling display.show() might help with choppy animations
// since those might interrupt the output and FastLED will retry a few times depending on the settings
//
// FastLED dithering is supported as well, which can also be executed with interrupts locked to a certain limit
// more information about this issue can be found here
// https://github.com/FastLED/FastLED/wiki/Interrupt-problems
// https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
//
// an additonal feature is the power limit to protect controller and LEDs. The FastLED library has been modified to configure the max.
// power consumption and a callback function has been added to display the calculated power usage, which is pretty close to the real values
//
// to reduce power consumtion and increase LED livespan when brightness is set to 0, all LEDs can be turned off which requires additional hardware to disconnect
// the LED strip from power (simple N-channel MOSFET disconnecting GND, for example AO3400, up to 5.8A) this can save a couple watts in standby mode
//
// The modified FastLED library can be found at https://github.com/sascha432/FastLED with a short documentation how to use the power limit and calculcations
//
// the code has not been tested on 8bit MCUs, with AVR there might be some issues since the STL library which is not complete and would required a custom
// implementation of std::bitset or it can be replaced by a simple array with the downside using 1 byte of memory instead of 1 bit

namespace SevenSegment {

    enum class SegmentType : uint8_t {
        NONE = 0,
        A = _BV(0),
        B = _BV(1),
        C = _BV(2),
        D = _BV(3),
        E = _BV(4),
        F = _BV(5),
        G = _BV(6),
        MAX = 7,

        DIGIT_0 = A|B|C|D|E|F,
        DIGIT_1 = B|C,
        DIGIT_2 = A|B|D|E|G,
        DIGIT_3 = A|B|C|D|G,
        DIGIT_4 = B|C|F|G,
        DIGIT_5 = A|C|D|F|G,
        DIGIT_6 = A|C|D|E|F|G,
        DIGIT_7 = A|B|C,
        DIGIT_8 = A|B|C|D|E|F|G,
        DIGIT_9 = A|B|C|D|F|G,
        DIGIT_A = A|B|C|E|F|G,      // A
        DIGIT_B = C|D|E|F|G,        // b
        DIGIT_C = A|D|E|F,          // C
        DIGIT_D = B|C|D|E|G,        // d
        DIGIT_E = A|D|E|F|G,        // E
        DIGIT_F = A|E|F|G,          // F
        MAX_DIGIT = 15
    };

    SegmentType getSegments(uint8_t digit);

    #if IOT_CLOCK_DISPLAY_INCLUDE == 1
        #include "display_clock.h"
    #elif IOT_CLOCK_DISPLAY_INCLUDE == 2
        #include "display_clockv2.h"
    #else
        #error "no translation table available"
    #endif

    extern "C" const SegmentType segmentTypeTranslationTable[] PROGMEM;
    extern "C" const PixelAddressType digitsTranslationTable[] PROGMEM;
    extern "C" const PixelAddressType colonTranslationTable[] PROGMEM;
    extern "C" const PixelAddressType pixelAnimationOrder[] PROGMEM;

    enum class ColonType : uint8_t {
        NONE = 0,
        TOP = 1,
        BOTTOM = 2,
        BOTH = TOP|BOTTOM,
        MAX
    };

    // inline static PixelAddressType readPixelAddress(PixelAddressPtr ptr) {
    //     if __CONSTEXPR17 (sizeof(PixelAddressType) == 2) {
    //         return pgm_read_word(ptr);
    //     }
    //     else {
    //         return pgm_read_byte(ptr);
    //     }
    // }

    inline __attribute__((__always_inline__))
    static bool operator==(SegmentType type, uint8_t segment) {
        return static_cast<uint8_t>(type) & _BV(segment);
    }

    inline __attribute__((__always_inline__))
    static bool operator==(ColonType type, uint8_t position) {
        return static_cast<uint8_t>(type) & _BV(position);
    }

    class Display : public BaseDisplayType {
    public:
        using DisplayType = BaseDisplayType;
        using CoordinateType = BaseDisplayType::CoordinateType;
        using PixelCoordinatesType = BaseDisplayType::PixelCoordinatesType;
        using ColorType = BaseDisplayType::ColorType;
        using BaseDisplayType::fill;

    public:
        // high level methods
        // -----------------------------------------------------------------

        /**
         * Format: 12:34.56
         *
         * Digits: 0-9, # = clear digit
         * Colons: . : or space = no colon
         *
         * "12:00"="12:00", "#1.1#""=" 1.1 ", "## 00"="   00", ...
         *
         * */
        inline __attribute__((__always_inline__))
        void print(const String &text) {
            print(text.c_str());
        }

        void print(const char *text) {
            if (!text || !*text) {
                hideAll();
                return;
            }
            uint8_t digit = 0;
            uint8_t colon = 0;
            while(*text) {
                if (isdigit(*text) || *text == '#') {
                    if (digit >= kNumDigits) {
                        break;
                    }
                    if (*text == '#') {
                        clearDigit(digit);
                    }
                    else {
                        setDigit(digit, *text - '0');
                    }
                    digit++;
                }
                else if (*text == ' ' || *text == '.' || *text == ':') {
                    if (colon < kNumColons) {
                        if (*text == ' ') {
                            clearColon(colon);
                        }
                        else if (*text == ':') {
                            setColon(colon, ColonType::BOTH);
                        }
                        else if (*text == '.') {
                            setColon(colon, ColonType::BOTTOM);
                        }
                        colon++;
                    }

                }
                text++;
            }
        }

        void setColon(uint8_t num, ColonType colon) {
            PixelAddressType buf[kNumPixelsPerColon * 2];
            memcpy_P(buf, getColonsArrayPtr(num), sizeof(buf));
            uint8_t n = 0;
            for(const auto address: buf) {
                setPixelState(address, (colon == (n / kNumPixelsPerColon)));
                n++;
            }
        }

        inline __attribute__((__always_inline__))
        void clearColon(uint8_t num) {
            setColon(num, ColonType::NONE);
        }

        void setColons(ColonType colon) {
            PixelAddressType buf[kNumPixels - kNumPixelsDigits];
            memcpy_P(buf, getColonsArrayPtr(0), sizeof(buf));
            uint8_t n = 0;
            for(const auto address: buf) {
                setPixelState(address, (colon == (n / kNumPixelsPerColon)));
                n++;
            }
        }

        inline __attribute__((__always_inline__))
        void clearColons() {
            setColons(ColonType::NONE);
        }

        void setDigit(uint8_t num, uint8_t digit) {
            #if DEBUG_IOT_CLOCK
                if (digit > static_cast<uint8_t>(SegmentType::MAX_DIGIT)) {
                    __DBG_panic("number %u out of range [0;%u]", digit, static_cast<uint8_t>(SegmentType::MAX_DIGIT));
                }
            #endif
            PixelAddressType buf[kNumPixelsPerDigit];
            memcpy_P(buf, getSegmentsArrayPtr(num), sizeof(buf));
            auto digitSegments = getSegments(digit);
            uint8_t n = 0;
            for(const auto address: buf) {
                setPixelState(address, (digitSegments == (n / kNumPixelsPerSegment)));
                n++;
            }
        }

        void clearDigit(uint8_t num) {
            PixelAddressType buf[kNumPixelsPerDigit];
            memcpy_P(buf, getSegmentsArrayPtr(num), sizeof(buf));
            for(const auto address: buf) {
                setPixelState(address, false);
            }
        }

        // low level methods
        // -----------------------------------------------------------------

        // make pixel visible
        inline __attribute__((__always_inline__))
        void setPixelState(PixelAddressType address, bool state) {
            _masked[address] = state;
        }

        inline __attribute__((__always_inline__))
        bool getPixelState(PixelAddressType address) const {
            return _masked[address];
        }

        inline __attribute__((__always_inline__))
        void hideAll() {
            _masked.reset();
        }

        inline __attribute__((__always_inline__))
        void showAll() {
            _masked.set();
        }

        void clear() {
            DisplayType::clear();
            hideAll();
        }

        void show() {
            _applyMask();
            BaseDisplayType::show(FastLED.getBrightness());
        }

        void show(uint8_t brightness) {
            _applyMask();
            BaseDisplayType::show(brightness);
        }

        void dump(Print &output) {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=clock brightness=%u\n"), __pixels.data(), _pixels, kPixelOffset, kNumPixels, FastLED.getBrightness());
        }

    private:
        inline __attribute__((__always_inline__))
        void _applyMask() {
            for(PixelAddressType i = 0; i < kNumPixels; i++) {
                if (!_masked[i]) {
                    setPixel(i, ColorType());
                }
            }
        }

        inline __attribute__((__always_inline__))
        PixelAddressPtr getColonsArrayPtr(uint8_t num) const {
            return &colonTranslationTable[(num * kNumPixelsPerColon * 2)];
        }

        inline __attribute__((__always_inline__))
        PixelAddressPtr getSegmentsArrayPtr(uint8_t num) const {
            return &digitsTranslationTable[(num * kNumPixelsPerDigit)];
        }

        inline __attribute__((__always_inline__))
        PixelAddressPtr getSegmentPixelAddressPtr(uint8_t num, uint8_t segment) const {
            return &(getSegmentsArrayPtr(num)[(segment * kNumPixelsPerSegment)]);
        }

    private:
        std::bitset<kNumPixels> _masked;

        static constexpr auto kSevenSegmentExtraMemorySize = sizeof(Display::_masked);
    };

    static constexpr auto kSevenSegmentTotalMemorySize = sizeof(Display);

    inline __attribute__((__always_inline__))
    SegmentType getSegments(uint8_t digit)
    {
        return static_cast<SegmentType>(pgm_read_byte(segmentTypeTranslationTable + digit));
    }

}

#pragma GCC pop_options

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif

#endif
