/**
 * Author: sascha_lammers@gmx.de
 */

#include "pixel_color.h"
#if ESP32
#    include <esp_attr.h>
#endif

namespace Clock {

    namespace PixelColorDetail {

        // reference values of FastLED's hsv2rgb_rainbow() at full saturation and value, the start of every
        // section and one value inside a section
        static_assert(hueToRGB(0) == 0xff0000, "red");
        static_assert(hueToRGB(32) == 0xab5500, "orange");
        static_assert(hueToRGB(64) == 0xabaa00, "yellow");
        static_assert(hueToRGB(96) == 0x00ff00, "green");
        static_assert(hueToRGB(128) == 0x00ab55, "aqua");
        static_assert(hueToRGB(160) == 0x0000ff, "blue");
        static_assert(hueToRGB(192) == 0x5500ab, "purple");
        static_assert(hueToRGB(224) == 0xaa0055, "pink");
        static_assert(hueToRGB(255) == 0xfd0002, "pink -> red, last hue");

        #if IOT_CLOCK_PIXEL_COLOR_HUE_TABLE
            // constant initialized at compile time, DRAM_ATTR keeps it out of the flash cache on the ESP32
            #if ESP32
                DRAM_ATTR
            #endif
            const HueTableType kHueTable = createHueTable();
        #endif

    }

    void hsv2rgb_rainbow(const PixelHSV *hsv, PixelRGB *rgb, uint16_t count)
    {
        const auto end = hsv + count;
        while (hsv < end) {
            *rgb++ = PixelColorDetail::hsvToRGB(hsv->hue, hsv->sat, hsv->val);
            hsv++;
        }
    }

    void nscale8(PixelRGB *pixels, uint16_t count, uint8_t scale)
    {
        if (scale == 255) {
            return;
        }
        // byte-wise over the buffer, the channels are scaled the same way and the layout of PixelRGB is 3 bytes
        const uint16_t factor = scale + 1;
        auto ptr = reinterpret_cast<uint8_t *>(pixels);
        const auto end = ptr + count * sizeof(PixelRGB);
        if (scale == 0) {
            std::fill(ptr, end, 0);
            return;
        }
        while (ptr < end) {
            *ptr = (*ptr * factor) >> 8;
            ptr++;
        }
    }

    void fadeToBlackBy(PixelRGB *pixels, uint16_t count, uint8_t fadeBy)
    {
        nscale8(pixels, count, 255 - fadeBy);
    }

}
