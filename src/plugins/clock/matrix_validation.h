/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "clock_def.h"

#if HAVE_NEOPIXELBUS && HAVE_NEOPIXELBUS_SUPPORT_MIC
#    include <driver/i2s.h> // I2S_NUM_0/I2S_NUM_1, the value of IOT_LED_MATRIX_I2S_PORT
#endif

// Single place that validates the LED matrix / LED strip layout.
//
// The LED buffer holds IOT_CLOCK_NUM_PIXELS pixels, the matrix mapping translates a logical pixel
// index (0 .. rows*cols-1) into that buffer. Every segment (one per output pin) is a panel of
// rows x cols LEDs placed in the matrix (top row / left column), see SegmentPanel.
//
// Rules:
//  - matrix rows * cols must fit into the buffer    -> error, the mapping would read outside the buffer
//  - a panel needs at least 1x1 LEDs                 -> error
//  - a panel must fit into the buffer (rows * cols)  -> error, its LUT would not fit
//  - a panel must not reach outside the matrix       -> error, those LEDs could never be drawn
//  - more enabled panels than output pins            -> error
//  - two panels sharing matrix pixels                -> warning only (the same pixels on two pins)
//  - matrix pixels no panel covers                   -> warning only
//
// Transport specific rules (the form shows them while editing, see Resources/js/forms/led-matrix.js):
//  - I2S with no segment          -> warning, nothing is transmitted
//  - I2S with more segments than free I2S ports -> info, the rest is transmitted by RMT (mixed mode)
//  - a segment larger than half the buffer -> info, its DMA buffer may not fit into the internal
//    DMA capable RAM and the segment is then transmitted by RMT
//  - FastLED/NeoPixelEx with more than one segment -> warning, only one chain is
//    driven (these builds have no I2S LED transport)
//
// The web form reports the errors to the user, ClockPlugin::_sanitizeConfig() keeps values that could
// read/write outside the buffer from reaching the driver.
namespace Clock {
    namespace MatrixValidation {

        static constexpr uint8_t kMaxSegments = 4;                  // one segment per output pin
        static constexpr uint32_t kMaxPixels = IOT_CLOCK_NUM_PIXELS; // pixel buffer size

        // how many segments/pins the transports can drive, shared with pixel_output_neobus.h
        // (NeoBusStrips/NeoBusRmtMux) so the form, _sanitizeConfig() and the driver use one source
        static constexpr uint8_t kMaxStrips = 4;                    // one segment per output pin
        #if HAVE_NEOPIXELBUS
            static constexpr uint8_t kMaxI2sStrips = 2;             // ESP32 has I2S0 and I2S1
        #else
            static constexpr uint8_t kMaxI2sStrips = 0;             // no I2S LED transport
        #endif
        // I2S port of the visualizer microphone, reserved for it and never used by the LED
        // transport (0xff if the microphone does not use one)
        #if HAVE_NEOPIXELBUS && HAVE_NEOPIXELBUS_SUPPORT_MIC
            static constexpr uint8_t kMicI2sPort = static_cast<uint8_t>(IOT_LED_MATRIX_I2S_PORT);
        #else
            static constexpr uint8_t kMicI2sPort = 0xff;
        #endif
        // I2S ports left for the LED transport after the microphone reserved one
        static constexpr uint8_t kAvailableI2sStrips = (kMaxI2sStrips && kMicI2sPort != 0xff) ? static_cast<uint8_t>(kMaxI2sStrips - 1) : kMaxI2sStrips;
        // a segment of this size may not fit into the internal DMA capable RAM of the I2S transport
        static constexpr uint32_t kLargeSegmentPixels = kMaxPixels / 2;

        struct Segment {
            bool enabled;
            uint16_t rows;      // panel size as it is mounted
            uint16_t cols;
            uint16_t posRow;    // placement in the matrix: top row / left column
            uint16_t posCol;

            uint32_t pixels() const {
                return enabled ? static_cast<uint32_t>(rows) * cols : 0;
            }

            // the panels overlap if their areas intersect
            bool intersects(const Segment &other) const {
                return posRow < static_cast<uint32_t>(other.posRow) + other.rows && other.posRow < static_cast<uint32_t>(posRow) + rows &&
                    posCol < static_cast<uint32_t>(other.posCol) + other.cols && other.posCol < static_cast<uint32_t>(posCol) + cols;
            }
        };

        struct Result {
            Segment segments[kMaxSegments];
            uint16_t rows;
            uint16_t cols;
            uint32_t matrixPixels;      // rows * cols
            uint32_t segmentPixels;     // sum of the LEDs of the enabled panels
            uint32_t maxPixels;         // pixel buffer size
            uint8_t activeSegments;     // enabled panels
            bool sizeExceeded;          // rows * cols > maxPixels
            bool panelInvalid;          // an enabled panel with 0 rows or columns
            uint8_t panelInvalidIndex;  // 1 based, 0 = none
            bool panelTooLarge;         // an enabled panel with more LEDs than the buffer
            uint8_t panelTooLargeIndex;
            bool panelOutside;          // an enabled panel reaches outside the matrix
            uint8_t panelOutsideIndex;
            bool overlap;               // two enabled panels share matrix pixels
            uint8_t overlapA;           // 1 based segment index (the earlier one)
            uint8_t overlapB;           // 1 based segment index (the later one)
            bool transportExceeded;     // activeSegments > kMaxStrips

            bool hasErrors() const {
                // rows/cols = 0 is not a buffer overflow, the mapping cannot be built
                return rows == 0 || cols == 0 || sizeExceeded || panelInvalid || panelTooLarge || panelOutside || transportExceeded;
            }

            // LEDs of the panels above (+) or below (-) the number of matrix pixels
            int32_t coverageDiff() const {
                return static_cast<int32_t>(segmentPixels) - static_cast<int32_t>(matrixPixels);
            }
        };

        inline Result validate(uint16_t rows, uint16_t cols, const Segment *segments, uint8_t count, uint32_t maxPixels)
        {
            Result result = {};
            result.rows = rows;
            result.cols = cols;
            result.matrixPixels = static_cast<uint32_t>(rows) * static_cast<uint32_t>(cols);
            result.maxPixels = maxPixels;
            result.sizeExceeded = result.matrixPixels > maxPixels;

            if (count > kMaxSegments) {
                count = kMaxSegments;
            }
            for (uint8_t i = 0; i < count; i++) {
                result.segments[i] = segments[i];
                if (!segments[i].enabled) {
                    continue;
                }
                result.activeSegments++;
                result.segmentPixels += segments[i].pixels();
                if ((!segments[i].rows || !segments[i].cols) && !result.panelInvalid) {
                    result.panelInvalid = true;
                    result.panelInvalidIndex = i + 1;
                }
                if (segments[i].pixels() > maxPixels && !result.panelTooLarge) {
                    result.panelTooLarge = true;
                    result.panelTooLargeIndex = i + 1;
                }
                if ((static_cast<uint32_t>(segments[i].posRow) + segments[i].rows > rows || static_cast<uint32_t>(segments[i].posCol) + segments[i].cols > cols) && !result.panelOutside) {
                    result.panelOutside = true;
                    result.panelOutsideIndex = i + 1;
                }
            }

            result.transportExceeded = result.activeSegments > kMaxStrips;

            // report the first overlapping pair
            for (uint8_t i = 1; i < count && !result.overlap; i++) {
                if (!result.segments[i].pixels()) {
                    continue;
                }
                for (uint8_t j = 0; j < i; j++) {
                    if (!result.segments[j].pixels()) {
                        continue;
                    }
                    if (result.segments[i].intersects(result.segments[j])) {
                        result.overlap = true;
                        result.overlapA = j + 1;
                        result.overlapB = i + 1;
                        break;
                    }
                }
            }
            return result;
        }

    }
}
