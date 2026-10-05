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
// The LED buffer holds IOT_CLOCK_NUM_PIXELS pixels. The matrix mapping translates a logical pixel
// index (0 .. rows*cols-1) into that buffer and every segment (one per output pin) describes a run
// of LEDs inside the buffer (offset + pixels).
//
// Rules:
//  - rows * cols must fit into the buffer      -> error, the mapping would read outside the buffer
//  - offset + pixels must fit into the buffer  -> error, the segment would read outside the buffer
//  - two segments must not overlap             -> error, the same pixels would be driven twice
//  - gaps are allowed and the segments do not have to cover rows * cols -> warning only
//  - a segment outside the matrix area (IOT_LED_MATRIX_PIXEL_OFFSET + rows * cols) -> warning only, the
//    form compares the ranges, not the sums
//
// Transport specific rules (the form shows them while editing, see Resources/js/forms/led-matrix.js):
//  - I2S with no segment          -> warning, nothing is transmitted
//  - I2S with more segments than free I2S ports -> info, the rest is transmitted by RMT (mixed mode)
//  - a segment larger than half the buffer -> info, its DMA buffer may not fit into the internal
//    DMA capable RAM and the segment is then transmitted by RMT
//  - FastLED/NeoPixelEx with more than one segment -> warning, only one chain is
//    driven (these builds have no I2S LED transport)
//
// The web form reports the errors to the user, ClockPlugin::_sanitizeConfig() uses clampSegment() to
// keep values that could read/write outside the buffer from reaching the driver.
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
            uint16_t offset;
            uint16_t pixels;
        };

        struct Result {
            Segment segments[kMaxSegments];
            uint16_t rows;
            uint16_t cols;
            uint32_t matrixPixels;      // rows * cols
            uint32_t segmentPixels;     // sum of the pixels of all segments
            uint32_t maxPixels;         // pixel buffer size
            uint8_t activeSegments;     // segments with pixels != 0
            bool sizeExceeded;          // rows * cols > maxPixels
            bool segmentExceeded;       // offset + pixels > maxPixels
            uint8_t segmentExceededIndex;   // 1 based, 0 = none

            bool overlap;               // two segments intersect
            uint8_t overlapA;           // 1 based segment index (the earlier one)
            uint8_t overlapB;           // 1 based segment index (the later one)
            bool transportExceeded;     // activeSegments > kMaxStrips

            bool hasErrors() const {
                // rows/cols = 0 is not a buffer overflow, the mapping cannot be built
                return rows == 0 || cols == 0 || sizeExceeded || segmentExceeded || overlap || transportExceeded;
            }

            // configured pixels above (+) or below (-) the number the matrix requires
            int32_t coverageDiff() const {
                return static_cast<int32_t>(segmentPixels) - static_cast<int32_t>(matrixPixels);
            }

            // one past the last pixel of a segment, index is 0 based
            uint32_t segmentEnd(uint8_t index) const {
                return static_cast<uint32_t>(segments[index].offset) + segments[index].pixels;
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
                result.segmentPixels += segments[i].pixels;
                if (segments[i].pixels) {
                    result.activeSegments++;
                    if (static_cast<uint32_t>(segments[i].offset) + segments[i].pixels > maxPixels && !result.segmentExceeded) {
                        result.segmentExceeded = true;
                        result.segmentExceededIndex = i + 1;
                    }
                }
            }

            result.transportExceeded = result.activeSegments > kMaxStrips;

            // report the first overlapping pair
            for (uint8_t i = 1; i < count && !result.overlap; i++) {
                if (!result.segments[i].pixels) {
                    continue;
                }
                const uint32_t endI = result.segmentEnd(i);
                for (uint8_t j = 0; j < i; j++) {
                    if (!result.segments[j].pixels) {
                        continue;
                    }
                    if (result.segments[i].offset < result.segmentEnd(j) && result.segments[j].offset < endI) {
                        result.overlap = true;
                        result.overlapA = j + 1;
                        result.overlapB = i + 1;
                        break;
                    }
                }
            }
            return result;
        }

        // Clamp a segment so it stays inside the pixel buffer. A segment that starts behind the
        // buffer is disabled, a segment that reaches beyond it is shortened. The result is returned
        // by value because the config stores the segments as bit fields that cannot be referenced.
        inline Segment clampSegment(uint16_t offset, uint16_t pixels, uint32_t maxPixels)
        {
            Segment segment;
            if (offset >= maxPixels) {
                segment.offset = 0;
                segment.pixels = 0;
            }
            else {
                segment.offset = offset;
                const uint32_t available = maxPixels - offset;
                segment.pixels = (pixels > available) ? static_cast<uint16_t>(available) : pixels;
            }
            return segment;
        }

    }
}
