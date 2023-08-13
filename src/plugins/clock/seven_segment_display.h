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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#pragma GCC push_options
#pragma GCC optimize ("O3")

// class for 7 segment display with various shapes that can be created with
// the ./scripts/tools/create_7segment_display.py tool
//
// currently up to 6 digits and 2 colons are supported
// the mapping is stored in PROGMEM to reduce memory overhead, the additional memory required is (total pixels / 8) bytes (32bit padded)
// see kSevenSegmentExtraMemorySize and kSevenSegmentTotalMemorySize (that is including the base class)
//
// high level functions to display the digits and colons or single dots are available
// low level functions for animations are also supported by the base class

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

    enum class ColonType : uint8_t {
        NONE = 0,
        TOP = 1,
        BOTTOM = 2,
        BOTH = TOP|BOTTOM,
        MAX
    };

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
        using BaseDisplayType::kNumPixels;

    public:
        // high level methods to display digits or colons
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
        void print(const String &text)
        {
            print(text.c_str());
        }

        void print(const char *text)
        {
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
        void setColon(uint8_t num, ColonType colon)
        {
            auto ptr = getColonsArrayPtr(num);
            for(int n = 0; n < (kNumPixelsPerColon * 2); n++) {
                setPixelState(pgm_read_data(ptr++), (colon == (n / kNumPixelsPerColon)));
            }
        }

        void setColons(ColonType colon)
        {
            auto ptr = getColonsArrayPtr(0);
            for(int n = 0; n < (kNumPixels - kNumPixelsDigits); n++) {
                setPixelState(pgm_read_data(ptr++), (colon == (n / kNumPixelsPerColon)));
            }
        }

        void setDigit(uint8_t num, uint8_t digit)
        {
            auto ptr = getSegmentsArrayPtr(num);
            auto digitSegments = getSegments(digit);
            for(int n = 0; n < kNumPixelsPerDigit; n++) {
                setPixelState(pgm_read_data(ptr++), (digitSegments == (n / kNumPixelsPerSegment)));
            }
        }

        void clearDigit(uint8_t num)
        {
            auto ptr = getSegmentsArrayPtr(num);
            for(int n = 0; n < kNumPixelsPerDigit; n++) {
                setPixelState(pgm_read_data(ptr++), false);
            }
        }

        inline __attribute__((__always_inline__))
        void clearColon(uint8_t num)
        {
            setColon(num, ColonType::NONE);
        }

        inline __attribute__((__always_inline__))
        void clearColons()
        {
            setColons(ColonType::NONE);
        }

        // low level methods for direct display access
        // -----------------------------------------------------------------

        // make pixel visible
        inline __attribute__((__always_inline__))
        void setPixelState(PixelAddressType address, bool state)
        {
            #if DEBUG_IOT_CLOCK
                if (address >= _masked.size()) {
                    __DBG_printf("addr=%u out of range=%u", address, _masked.size());
                    return;
                }
            #endif
            _masked[address] = state;
        }

        inline __attribute__((__always_inline__))
        bool getPixelState(PixelAddressType address) const
        {
            return _masked[address];
        }

        inline __attribute__((__always_inline__))
        void hideAll()
        {
            _masked.reset();
        }

        inline __attribute__((__always_inline__))
        void showAll()
        {
            _masked.set();
        }

        void clear()
        {
            DisplayType::clear();
            hideAll();
        }

        void show()
        {
            _applyMask();
            BaseDisplayType::show(FastLED.getBrightness());
        }

        void show(uint8_t brightness)
        {
            _applyMask();
            BaseDisplayType::show(brightness);
        }

        void dump(Print &output)
        {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=clock brightness=%u\n"), __pixels.data(), _pixels, kPixelOffset, kNumPixels, FastLED.getBrightness());
        }

    private:
        inline __attribute__((__always_inline__))
        void _applyMask()
        {
            for(PixelAddressType i = 0; i < kNumPixels; i++) {
                if (!_masked[i]) {
                    setPixel(i, ColorType());
                }
            }
        }

        // helper methods to get the PROGMEM table pointers

        inline __attribute__((__always_inline__))
        static PixelAddressPtr getColonsArrayPtr(uint8_t num)
        {
            return &colonTranslationTable[(num * kNumPixelsPerColon * 2)];
        }

        inline __attribute__((__always_inline__))
        static PixelAddressPtr getSegmentsArrayPtr(uint8_t num)
        {
            return &digitsTranslationTable[(num * kNumPixelsPerDigit)];
        }

        inline __attribute__((__always_inline__))
        static PixelAddressPtr getSegmentPixelAddressPtr(uint8_t num, uint8_t segment)
        {
            return &(getSegmentsArrayPtr(num)[(segment * kNumPixelsPerSegment)]);
        }

        inline __attribute__((__always_inline__))
        SegmentType getSegments(uint8_t digit)
        {
            #if DEBUG_IOT_CLOCK
                if (digit > static_cast<uint8_t>(SegmentType::MAX_DIGIT)) {
                    __DBG_panic("number %u out of range [0;%u]", digit, SegmentType::MAX_DIGIT);
                }
            #endif
            return pgm_read_data(&segmentTypeTranslationTable[digit]);
        }

        // helper methods to read from PROGMEM without knowing the actual type

        template<typename _Ta, typename std::enable_if<sizeof(_Ta) == sizeof(uint8_t), int>::type = 0>
        __attribute__((__always_inline__))
        static _Ta pgm_read_data(const _Ta *ptr)
        {
            return static_cast<_Ta>(pgm_read_byte(reinterpret_cast<const uint8_t *>(ptr)));
        }

        template<typename _Ta, typename std::enable_if<sizeof(_Ta) == sizeof(uint16_t), int>::type = 0>
        __attribute__((__always_inline__))
        static _Ta pgm_read_data(const _Ta *ptr)
        {
            return static_cast<_Ta>(pgm_read_word(reinterpret_cast<const uint16_t *>(ptr)));
        }

        template<typename _Ta, typename std::enable_if<sizeof(_Ta) == sizeof(uint32_t), int>::type = 0>
        __attribute__((__always_inline__))
        static _Ta pgm_read_data(const _Ta *ptr)
        {
            return static_cast<_Ta>(pgm_read_dword_aligned(reinterpret_cast<const uint32_t *>(ptr)));
        }

    private:
        std::bitset<kNumPixels> _masked;

        static constexpr auto kSevenSegmentExtraMemorySize = sizeof(Display::_masked);
    };

    static constexpr auto kSevenSegmentTotalMemorySize = sizeof(Display);

}

#pragma GCC pop_options

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif

#endif
