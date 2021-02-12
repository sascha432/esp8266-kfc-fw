/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "pixel_display.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace SevenSegment {

    enum class SegmentType : uint8_t {
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

    static inline bool operator==(SegmentType type, uint8_t segment) {
        return static_cast<uint8_t>(type) & _BV(segment);
    }

    enum class ColonType : uint8_t {
        TOP = 0,
        BOTTOM,
        MAX
    };

    using SegmentTypeArray = std::array<SegmentType, 16>; // digits 0-F, bitset SegmentType

    static constexpr auto kSegmentTypeTranslationTable = SegmentTypeArray({ SegmentType::DIGIT_0, SegmentType::DIGIT_1, SegmentType::DIGIT_2, SegmentType::DIGIT_3, SegmentType::DIGIT_4, SegmentType::DIGIT_5, SegmentType::DIGIT_6, SegmentType::DIGIT_7, SegmentType::DIGIT_8, SegmentType::DIGIT_9, SegmentType::DIGIT_A, SegmentType::DIGIT_B, SegmentType::DIGIT_C, SegmentType::DIGIT_D, SegmentType::DIGIT_E, SegmentType::DIGIT_F });

    #if IOT_CLOCK_DISPLAY_INCLUDE == 2

        #include "display_clockv2.h"

    #else

        #error "no translation table available"

    #endif

    class Display : public BaseDisplayType {
    public:
        using DisplayType = BaseDisplayType;

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
                    if (digit >= kNumDigits) {
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
                    if (colon < kNumColons) {
                        if (*text == ' ') {
                            clearColon(colon, ColonType::TOP);
                            clearColon(colon, ColonType::BOTTOM);
                        }
                        else if (*text == ':') {
                            setColon(colon, ColonType::TOP, color);
                            setColon(colon, ColonType::BOTTOM, color);
                        }
                        else if (*text == '.') {
                            clearColon(colon, ColonType::TOP);
                            setColon(colon, ColonType::BOTTOM, color);
                        }
                        colon++;
                    }

                }
                text++;
            }
        }

        void setColon(uint8_t num, ColonType colon, ColorType color) {
            for(const auto &pixels: kColonTranslationTable[static_cast<uint8_t>(colon)]) {
                for(const auto pixel: pixels) {
                    setPixel(pixel, color);
                }
            }
        }

        void clearColon(uint8_t num, ColonType colon) {
            setColon(num, colon, ColorType());
        }

        void setDigit(uint8_t num, uint8_t number, ColorType color) {
            #if DEBUG_IOT_CLOCK
                if (number > static_cast<uint8_t>(SegmentType::MAX_DIGIT)) {
                    __DBG_panic("number %u out of range [0;%u]", number, static_cast<uint8_t>(SegmentType::MAX_DIGIT));
                }
            #endif
            auto numberSegments = kSegmentTypeTranslationTable[number];
            uint8_t n = 0;
            for(const auto &segments: kDigitsTranslationTable[num]) {
                for (const auto pixel: segments) {
                    setPixel(pixel, (numberSegments == n) ? color : ColorType());
                    // setPixel(pixel, (static_cast<uint8_t>(numberSegments) & _BV(n)) ? color : ColorType());
                }
                n++;
            }
        }

        void clearDigit(uint8_t num) {
            for(const auto &segments: kDigitsTranslationTable[num]) {
                for (const auto pixel: segments) {
                    setPixel(pixel, ColorType());
                }
            }
        }

    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
