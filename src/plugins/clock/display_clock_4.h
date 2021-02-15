// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: ./scripts/tools/create_7segment_display.py dumpcode --name clock_4

#pragma once

#include <stdint.h>
#include <array>

using PixelAddressType = uint8_t;

using SegmentPixelArray = std::array<PixelAddressType, 2>;
using SegmentArray = std::array<SegmentPixelArray, 7>;
using DigitArray = std::array<SegmentArray, 4>;

using ColonPixelsArray = std::array<PixelAddressType, 1>;
using ColonPixelArray = std::array<ColonPixelsArray, 2>;
using ColonArray = std::array<ColonPixelArray, 1>;

using AnimationOrderArray = std::array<PixelAddressType, 14>;

static constexpr uint8_t kNumDigits = 4;
#define IOT_CLOCK_NUM_DIGITS 4
static constexpr uint8_t kNumColons = 1;
#define IOT_CLOCK_NUM_COLONS 1
static constexpr uint8_t kNumPixels = 58;
static constexpr uint8_t kNumPixelsDigits = 56;

static constexpr auto kDigitsTranslationTable = DigitArray({
    SegmentArray({
        SegmentPixelArray({ 0, 1 }),
        SegmentPixelArray({ 2, 3 }),
        SegmentPixelArray({ 6, 7 }),
        SegmentPixelArray({ 8, 9 }),
        SegmentPixelArray({ 10, 11 }),
        SegmentPixelArray({ 12, 13 }),
        SegmentPixelArray({ 4, 5 })
    }),
    SegmentArray({
        SegmentPixelArray({ 14, 15 }),
        SegmentPixelArray({ 16, 17 }),
        SegmentPixelArray({ 20, 21 }),
        SegmentPixelArray({ 22, 23 }),
        SegmentPixelArray({ 24, 25 }),
        SegmentPixelArray({ 26, 27 }),
        SegmentPixelArray({ 18, 19 })
    }),
    SegmentArray({
        SegmentPixelArray({ 30, 31 }),
        SegmentPixelArray({ 32, 33 }),
        SegmentPixelArray({ 36, 37 }),
        SegmentPixelArray({ 38, 39 }),
        SegmentPixelArray({ 40, 41 }),
        SegmentPixelArray({ 42, 43 }),
        SegmentPixelArray({ 34, 35 })
    }),
    SegmentArray({
        SegmentPixelArray({ 44, 45 }),
        SegmentPixelArray({ 46, 47 }),
        SegmentPixelArray({ 50, 51 }),
        SegmentPixelArray({ 52, 53 }),
        SegmentPixelArray({ 54, 55 }),
        SegmentPixelArray({ 56, 57 }),
        SegmentPixelArray({ 48, 49 })
    })
});

static constexpr auto kColonTranslationTable = ColonArray({
    ColonPixelArray({
        ColonPixelsArray({ 28 }),
        ColonPixelsArray({ 30 })
    })
});

static constexpr auto kPixelAnimationOrder = AnimationOrderArray({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13});

