/**
 * Author: sascha_lammers@gmx.de
 */

// Pixel type and color math of the clock plugin, replaces the parts of FastLED that the plugin uses for its
// pixel buffer and animations:
//
//  FastLED                         replacement
//  CRGB                            Clock::PixelRGB
//  CHSV                            Clock::PixelHSV
//  fract8, accum88, saccum87       Clock::fract8, Clock::accum88, Clock::saccum87
//  hsv2rgb_rainbow()               Clock::hsv2rgb_rainbow(), PixelRGB(const PixelHSV &)
//  scale8(), scale8_video()        Clock::scale8(), Clock::scale8_video()
//  blend8()                        Clock::blend8()
//  blend(CRGB, CRGB, fract8)       Clock::blend()
//  nblend(CRGB &, CRGB, fract8)    Clock::nblend()
//  CRGB::nscale8()                 PixelRGB::nscale8()
//  fadeToBlackBy()                 Clock::fadeToBlackBy()
//  beat8(), beat16(), beat88()     Clock::beat8(), Clock::beat16(), Clock::beat88()
//
// The results are bit-identical to FastLED 3.4 (fork) and 3.9.20 (official), both are built with
// FASTLED_SCALE8_FIXED=1 and the "Y1" yellow boost of hsv2rgb_rainbow(). The memory layout of PixelRGB is
// the same as CRGB (3 bytes, red/green/blue), so the FastLED and NeoPixelEx transports can still read the
// pixel buffer.
//
// The names live in the Clock namespace: code inside the namespace finds them before FastLED's global
// functions, code outside (ClockPlugin) has to qualify them with Clock::. A class that has a member with the
// same name (Animation::blend()) has to qualify the call as well.
//
// ESP32: the hue wheel of hsv2rgb_rainbow() is a 256 entry table in DRAM (1 KB), one load instead of the
// branches and multiplications. ESP8266: the hue wheel is calculated, the RAM is too precious for the table.
// IOT_CLOCK_PIXEL_COLOR_HUE_TABLE overrides the default of the platform.

#pragma once

#include <Arduino_compat.h>
#include <array>
#include <stddef.h>
#include <stdint.h>

#ifndef IOT_CLOCK_PIXEL_COLOR_HUE_TABLE
#    if ESP32
#        define IOT_CLOCK_PIXEL_COLOR_HUE_TABLE 1
#    else
#        define IOT_CLOCK_PIXEL_COLOR_HUE_TABLE 0
#    endif
#endif

namespace Clock {

    // fraction of 256, 0 = 0.0, 255 = 0.996
    using fract8 = uint8_t;
    // unsigned fixed point, 8 bit integer and 8 bit fraction
    using accum88 = uint16_t;
    // signed fixed point, 8 bit integer and 7 bit fraction
    using saccum87 = int16_t;

    // ------------------------------------------------------------------------
    // 8 bit math
    // ------------------------------------------------------------------------

    // i * (scale + 1) / 256: scale8(255, 255) is 255 (FASTLED_SCALE8_FIXED=1)
    constexpr uint8_t scale8(uint8_t i, fract8 scale)
    {
        return (i * (scale + 1)) >> 8;
    }

    // i * scale / 256, a value that is not 0 stays above 0 unless scale is 0
    constexpr uint8_t scale8_video(uint8_t i, fract8 scale)
    {
        return ((i * scale) >> 8) + ((i && scale) ? 1 : 0);
    }

    // (a * (256 - amountOfB) + b * (amountOfB + 1)) / 256, with a single multiplication
    // amountOfB 0 returns a, 255 returns b
    constexpr uint8_t blend8(uint8_t a, uint8_t b, fract8 amountOfB)
    {
        return ((a << 8) + b + (b - a) * amountOfB) >> 8;
    }

    // ------------------------------------------------------------------------
    // beat generators, sawtooth waves based on millis()
    // ------------------------------------------------------------------------

    // 16 bit sawtooth, beatsPerMinute88 in Q8.8. 60000 ms are converted to 65536 ms with * 280 / 256
    // (0.05% error, 120 BPM are 119.93 BPM)
    inline uint16_t beat88(accum88 beatsPerMinute88, uint32_t timebase = 0)
    {
        return ((static_cast<uint32_t>(millis()) - timebase) * beatsPerMinute88 * 280U) >> 16;
    }

    // 16 bit sawtooth, a value below 256 is BPM, otherwise Q8.8
    inline uint16_t beat16(accum88 beatsPerMinute, uint32_t timebase = 0)
    {
        if (beatsPerMinute < 256) {
            beatsPerMinute <<= 8;
        }
        return beat88(beatsPerMinute, timebase);
    }

    // 8 bit sawtooth, a value below 256 is BPM, otherwise Q8.8
    inline uint8_t beat8(accum88 beatsPerMinute, uint32_t timebase = 0)
    {
        return beat16(beatsPerMinute, timebase) >> 8;
    }

    // ------------------------------------------------------------------------
    // hue wheel of hsv2rgb_rainbow()
    // ------------------------------------------------------------------------

    namespace PixelColorDetail {

        // 0x00RRGGBB for a hue at full saturation and value: 8 sections of 32 hues each, yellow is boosted
        // to look as bright as the other colors (FastLED's "Y1")
        constexpr uint32_t hueToRGB(uint8_t hue)
        {
            const uint8_t offset8 = (hue & 0x1f) << 3;
            const uint8_t third = scale8(offset8, 256 / 3); // max. 85
            const uint8_t twoThirds = scale8(offset8, (256 * 2) / 3); // max. 170
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            switch (hue >> 5) {
            case 0: // red -> orange
                r = 255 - third;
                g = third;
                break;
            case 1: // orange -> yellow
                r = 171;
                g = 85 + third;
                break;
            case 2: // yellow -> green
                r = 171 - twoThirds;
                g = 170 + third;
                break;
            case 3: // green -> aqua
                g = 255 - third;
                b = third;
                break;
            case 4: // aqua -> blue
                g = 171 - twoThirds;
                b = 85 + twoThirds;
                break;
            case 5: // blue -> purple
                r = third;
                b = 255 - third;
                break;
            case 6: // purple -> pink
                r = 85 + third;
                b = 171 - third;
                break;
            default: // pink -> red
                r = 170 + third;
                b = 85 - third;
                break;
            }
            return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
        }

        using HueTableType = std::array<uint32_t, 256>;

        constexpr HueTableType createHueTable()
        {
            HueTableType table {};
            for (uint16_t hue = 0; hue < table.size(); hue++) {
                table[hue] = hueToRGB(hue);
            }
            return table;
        }

        #if IOT_CLOCK_PIXEL_COLOR_HUE_TABLE
            // defined in pixel_color.cpp, DRAM on the ESP32
            extern const HueTableType kHueTable;
        #endif

        inline uint32_t getHueRGB(uint8_t hue)
        {
            #if IOT_CLOCK_PIXEL_COLOR_HUE_TABLE
                return kHueTable[hue];
            #else
                return hueToRGB(hue);
            #endif
        }

        // hsv2rgb_rainbow() as 0x00RRGGBB
        inline uint32_t hsvToRGB(uint8_t hue, uint8_t sat, uint8_t val)
        {
            uint32_t rgb = getHueRGB(hue);
            if (sat != 255) {
                if (sat == 0) {
                    rgb = 0xffffff;
                }
                else {
                    // scale down to the saturation and add the same amount as brightness floor, the sum of
                    // a channel cannot exceed 255
                    uint8_t desat = 255 - sat;
                    desat = scale8_video(desat, desat);
                    const uint8_t satScale = 255 - desat;
                    rgb = (static_cast<uint32_t>(scale8(rgb >> 16, satScale) + desat) << 16) |
                        (static_cast<uint32_t>(scale8(rgb >> 8, satScale) + desat) << 8) |
                        static_cast<uint8_t>(scale8(rgb, satScale) + desat);
                }
            }
            if (val != 255) {
                val = scale8_video(val, val);
                if (val == 0) {
                    return 0;
                }
                rgb = (static_cast<uint32_t>(scale8(rgb >> 16, val)) << 16) |
                    (static_cast<uint32_t>(scale8(rgb >> 8, val)) << 8) |
                    scale8(rgb, val);
            }
            return rgb;
        }

    }

    // ------------------------------------------------------------------------
    // PixelHSV
    // ------------------------------------------------------------------------

    struct PixelHSV {
        union {
            struct {
                union {
                    uint8_t hue;
                    uint8_t h;
                };
                union {
                    uint8_t sat;
                    uint8_t s;
                };
                union {
                    uint8_t val;
                    uint8_t v;
                };
            };
            uint8_t raw[3];
        };

        // all 0, as CHSV
        constexpr PixelHSV();
        constexpr PixelHSV(uint8_t hue, uint8_t sat, uint8_t val);

        uint8_t &operator[](uint8_t index);
        const uint8_t &operator[](uint8_t index) const;
    };

    static_assert(sizeof(PixelHSV) == 3, "PixelHSV must have the size of CHSV");

    constexpr PixelHSV::PixelHSV() :
        h(0),
        s(0),
        v(0)
    {
    }

    constexpr PixelHSV::PixelHSV(uint8_t hue, uint8_t sat, uint8_t val) :
        h(hue),
        s(sat),
        v(val)
    {
    }

    inline uint8_t &PixelHSV::operator[](uint8_t index)
    {
        return raw[index];
    }

    inline const uint8_t &PixelHSV::operator[](uint8_t index) const
    {
        return raw[index];
    }

    inline bool operator==(const PixelHSV &lhs, const PixelHSV &rhs)
    {
        return lhs.h == rhs.h && lhs.s == rhs.s && lhs.v == rhs.v;
    }

    inline bool operator!=(const PixelHSV &lhs, const PixelHSV &rhs)
    {
        return !(lhs == rhs);
    }

    // ------------------------------------------------------------------------
    // PixelRGB
    // ------------------------------------------------------------------------

    struct PixelRGB {
        union {
            struct {
                union {
                    uint8_t r;
                    uint8_t red;
                };
                union {
                    uint8_t g;
                    uint8_t green;
                };
                union {
                    uint8_t b;
                    uint8_t blue;
                };
            };
            uint8_t raw[3];
        };

        // uninitialized like CRGB, PixelRGB() and std::array::fill(PixelRGB()) set all channels to 0
        PixelRGB() = default;
        constexpr PixelRGB(uint8_t red, uint8_t green, uint8_t blue);
        // 0xRRGGBB
        constexpr PixelRGB(uint32_t colorCode);
        // hsv2rgb_rainbow()
        PixelRGB(const PixelHSV &hsv);

        PixelRGB &operator=(uint32_t colorCode);
        PixelRGB &operator=(const PixelHSV &hsv);

        uint8_t &operator[](uint8_t index);
        const uint8_t &operator[](uint8_t index) const;

        // true if any channel is not 0
        constexpr explicit operator bool() const;
        // 0xRRGGBB (FastLED 3.9 adds 0xff000000, the FastLED 3.4 fork has none and converts through its
        // implicit operator bool() to 0 or 1)
        constexpr explicit operator uint32_t() const;

        // 0xRRGGBB
        constexpr uint32_t toRGB() const;

        // every channel * (scale + 1) / 256
        PixelRGB &nscale8(uint8_t scale);
        // every channel * (256 - fadeBy) / 256
        PixelRGB &fadeToBlackBy(uint8_t fadeBy);
    };

    static_assert(sizeof(PixelRGB) == 3, "PixelRGB must have the size of CRGB, the transports read the pixel buffer as bytes");
    static_assert(offsetof(PixelRGB, r) == 0 && offsetof(PixelRGB, g) == 1 && offsetof(PixelRGB, b) == 2, "PixelRGB must have the layout of CRGB");

    constexpr PixelRGB::PixelRGB(uint8_t red, uint8_t green, uint8_t blue) :
        r(red),
        g(green),
        b(blue)
    {
    }

    constexpr PixelRGB::PixelRGB(uint32_t colorCode) :
        r(static_cast<uint8_t>(colorCode >> 16)),
        g(static_cast<uint8_t>(colorCode >> 8)),
        b(static_cast<uint8_t>(colorCode))
    {
    }

    inline PixelRGB::PixelRGB(const PixelHSV &hsv) :
        PixelRGB(PixelColorDetail::hsvToRGB(hsv.hue, hsv.sat, hsv.val))
    {
    }

    inline PixelRGB &PixelRGB::operator=(uint32_t colorCode)
    {
        r = colorCode >> 16;
        g = colorCode >> 8;
        b = colorCode;
        return *this;
    }

    inline PixelRGB &PixelRGB::operator=(const PixelHSV &hsv)
    {
        return *this = PixelColorDetail::hsvToRGB(hsv.hue, hsv.sat, hsv.val);
    }

    inline uint8_t &PixelRGB::operator[](uint8_t index)
    {
        return raw[index];
    }

    inline const uint8_t &PixelRGB::operator[](uint8_t index) const
    {
        return raw[index];
    }

    constexpr PixelRGB::operator bool() const
    {
        return r || g || b;
    }

    constexpr PixelRGB::operator uint32_t() const
    {
        return toRGB();
    }

    constexpr uint32_t PixelRGB::toRGB() const
    {
        return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    }

    inline PixelRGB &PixelRGB::nscale8(uint8_t scale)
    {
        const uint16_t factor = scale + 1;
        r = (r * factor) >> 8;
        g = (g * factor) >> 8;
        b = (b * factor) >> 8;
        return *this;
    }

    inline PixelRGB &PixelRGB::fadeToBlackBy(uint8_t fadeBy)
    {
        return nscale8(255 - fadeBy);
    }

    inline bool operator==(const PixelRGB &lhs, const PixelRGB &rhs)
    {
        return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
    }

    inline bool operator!=(const PixelRGB &lhs, const PixelRGB &rhs)
    {
        return !(lhs == rhs);
    }

    // ------------------------------------------------------------------------
    // color functions
    // ------------------------------------------------------------------------

    inline void hsv2rgb_rainbow(const PixelHSV &hsv, PixelRGB &rgb)
    {
        rgb = PixelColorDetail::hsvToRGB(hsv.hue, hsv.sat, hsv.val);
    }

    // blends overlay into existing, amountOfOverlay 0 keeps existing, 255 is overlay
    inline PixelRGB &nblend(PixelRGB &existing, const PixelRGB &overlay, fract8 amountOfOverlay)
    {
        existing.r = blend8(existing.r, overlay.r, amountOfOverlay);
        existing.g = blend8(existing.g, overlay.g, amountOfOverlay);
        existing.b = blend8(existing.b, overlay.b, amountOfOverlay);
        return existing;
    }

    // p1 blended with p2, amountOfP2 0 is p1, 255 is p2
    inline PixelRGB blend(const PixelRGB &p1, const PixelRGB &p2, fract8 amountOfP2)
    {
        return PixelRGB(blend8(p1.r, p2.r, amountOfP2), blend8(p1.g, p2.g, amountOfP2), blend8(p1.b, p2.b, amountOfP2));
    }

    // bulk versions, see pixel_color.cpp
    void hsv2rgb_rainbow(const PixelHSV *hsv, PixelRGB *rgb, uint16_t count);
    void nscale8(PixelRGB *pixels, uint16_t count, uint8_t scale);
    void fadeToBlackBy(PixelRGB *pixels, uint16_t count, uint8_t fadeBy);

}
