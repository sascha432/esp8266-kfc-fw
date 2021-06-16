/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX == 0

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <bitset>
#include "pixel_display.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

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
        TOP = 0,
        BOTTOM,
        MAX
    };

    // inline static PixelAddressType readPixelAddress(PixelAddressPtr ptr) {
    //     if constexpr (sizeof(PixelAddressType) == 2) {
    //         return pgm_read_word(ptr);
    //     }
    //     else {
    //         return pgm_read_byte(ptr);
    //     }
    // }

    inline static bool operator==(SegmentType type, uint8_t segment) {
        return static_cast<uint8_t>(type) & _BV(segment);
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
                            clearColon(colon, ColonType::TOP);
                            clearColon(colon, ColonType::BOTTOM);
                        }
                        else if (*text == ':') {
                            setColon(colon, ColonType::TOP);
                            setColon(colon, ColonType::BOTTOM);
                        }
                        else if (*text == '.') {
                            clearColon(colon, ColonType::TOP);
                            setColon(colon, ColonType::BOTTOM);
                        }
                        colon++;
                    }

                }
                text++;
            }
        }

        void setColon(uint8_t num, ColonType colon, bool state = true) {
            PixelAddressType buf[kNumPixelsPerColon];
            memcpy_P(buf, getColonsArrayPtr(num), kNumPixelsPerColon * sizeof(PixelAddressType));
            for(const auto address: buf) {
                setPixelState(address, state);
            }
        }

        void clearColon(uint8_t num, ColonType colon) {
            setColon(num, colon, false);
        }

        void setDigit(uint8_t num, uint8_t digit, int color = -1) {
            #if DEBUG_IOT_CLOCK
                if (digit > static_cast<uint8_t>(SegmentType::MAX_DIGIT)) {
                    __DBG_panic("number %u out of range [0;%u]", digit, static_cast<uint8_t>(SegmentType::MAX_DIGIT));
                }
            #endif
            PixelAddressType buf[kNumPixelsPerDigit];
            memcpy_P(buf, getSegmentsArrayPtr(num), sizeof(buf));
            if (digit == 0xff) {
                for(const auto address: buf) {
                    setPixelState(address, false);
                }
            }
            else {
                auto digitSegments = getSegments(digit);
                uint8_t n = 0;
                for(const auto address: buf) {
                    setPixelState(address, (digitSegments == n));
                    if (color != -1) {
                        setPixel(address, color);
                    }
                    n++;
                }
            }
        }

        void clearDigit(uint8_t num) {
            setDigit(num, 0xff);
        }

        // low level methods
        // -----------------------------------------------------------------

        // make pixel visible
        void setPixelState(PixelAddressType address, bool state) {
            _masked[address] = state;
        }

        bool getPixelState(PixelAddressType address) const {
            return _masked[address];
        }

        void hideAll() {
            _masked.reset();
        }

        void showAll() {
            _masked.set();
        }

        void clear() {
            DisplayType::clear();
            showAll();
        }

        void show() {
            _applyMask();
            FastLED.show();
        }

        void show(uint8_t brightness) {
            _applyMask();
            FastLED.show(brightness);
        }

        void dump(Print &output) {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=clock\n"), __pixels.data(), _pixels, kPixelOffset, kNumPixels);
        }

    private:
        void _applyMask() {
            for(PixelAddressType i = 0; i < kNumPixels; i++) {
                if (!_masked[i]) {
                    setPixel(i, ColorType());
                }
            }
        }

        PixelAddressPtr getColonsArrayPtr(uint8_t num) const {
            return &colonTranslationTable[(num * kNumPixelsPerColon)];
        }

        PixelAddressPtr getSegmentsArrayPtr(uint8_t num) const {
            return &digitsTranslationTable[(num * kNumPixelsPerDigit)];
        }

        PixelAddressPtr getSegmentPixelAddressPtr(uint8_t num, uint8_t segment) const {
            return &(getSegmentsArrayPtr(num)[(segment * kNumPixelsPerSegment)]);
        }

    private:
        std::bitset<kNumPixels> _masked;
    };

    inline SegmentType getSegments(uint8_t digit)
    {
        return static_cast<SegmentType>(pgm_read_byte(segmentTypeTranslationTable + digit));
    }

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif


#endif
