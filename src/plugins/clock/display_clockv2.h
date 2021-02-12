// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: scripts/tools/create_7segment_display.py dumpcode --name clockv2

#pragma once

#include <stdint.h>
#include <array>

using PixelAddressType = uint8_t;

using SegmentPixelArray = std::array<PixelAddressType, 4>;
using SegmentArray = std::array<SegmentPixelArray, 7>;
using DigitArray = std::array<SegmentArray, 4>;

using ColonPixelsArray = std::array<PixelAddressType, 2>;
using ColonPixelArray = std::array<ColonPixelsArray, 2>;
using ColonArray = std::array<ColonPixelArray, 1>;

using AnimationOrderArray = std::array<PixelAddressType, 32>;

static constexpr uint8_t kNumDigits = 4;
#define IOT_CLOCK_NUM_DIGITS 4
static constexpr uint8_t kNumColons = 1;
#define IOT_CLOCK_NUM_COLONS 1
static constexpr uint8_t kNumPixels = 116;
static constexpr uint8_t kNumPixelsDigits = 112;

static constexpr auto kDigitsTranslationTable = DigitArray({
    SegmentArray({
        SegmentPixelArray({ 112, 113, 114, 115 }),
        SegmentPixelArray({ 108, 109, 110, 111 }),
        SegmentPixelArray({ 96, 97, 98, 99 }),
        SegmentPixelArray({ 100, 101, 102, 103 }),
        SegmentPixelArray({ 104, 105, 106, 107 }),
        SegmentPixelArray({ 88, 89, 90, 91 }),
        SegmentPixelArray({ 92, 93, 94, 95 })
    }),
    SegmentArray({
        SegmentPixelArray({ 84, 85, 86, 87 }),
        SegmentPixelArray({ 80, 81, 82, 83 }),
        SegmentPixelArray({ 68, 69, 70, 71 }),
        SegmentPixelArray({ 72, 73, 74, 75 }),
        SegmentPixelArray({ 76, 77, 78, 79 }),
        SegmentPixelArray({ 60, 61, 62, 63 }),
        SegmentPixelArray({ 64, 65, 66, 67 })
    }),
    SegmentArray({
        SegmentPixelArray({ 28, 29, 30, 31 }),
        SegmentPixelArray({ 24, 25, 26, 27 }),
        SegmentPixelArray({ 12, 13, 14, 15 }),
        SegmentPixelArray({ 16, 17, 18, 19 }),
        SegmentPixelArray({ 20, 21, 22, 23 }),
        SegmentPixelArray({ 4, 5, 6, 7 }),
        SegmentPixelArray({ 8, 9, 10, 11 })
    }),
    SegmentArray({
        SegmentPixelArray({ 56, 57, 58, 59 }),
        SegmentPixelArray({ 52, 53, 54, 55 }),
        SegmentPixelArray({ 40, 41, 42, 43 }),
        SegmentPixelArray({ 44, 45, 46, 47 }),
        SegmentPixelArray({ 48, 49, 50, 51 }),
        SegmentPixelArray({ 32, 33, 34, 35 }),
        SegmentPixelArray({ 36, 37, 38, 39 })
    })
});

static constexpr auto kColonTranslationTable = ColonArray({
    ColonPixelArray({
        ColonPixelsArray({ 0, 1 }),
        ColonPixelsArray({ 4, 5 })
    })
});

static constexpr auto kPixelAnimationOrder = AnimationOrderArray({28, 29, 30, 31, 24, 25, 26, 27, 11, 10, 9, 8, 20, 21, 22, 23, 16, 17, 18, 19, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4});

