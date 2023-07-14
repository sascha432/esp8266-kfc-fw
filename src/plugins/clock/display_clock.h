// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: ./scripts/tools/create_7segment_display.py dumpcode --name clock

#pragma once

#include <Arduino_compat.h>
#include <stdint.h>
#include <array>

using PixelAddressType = uint8_t;
using PixelAddressPtr = const PixelAddressType *;

static constexpr PixelAddressType kNumDigits = 6;
static constexpr PixelAddressType kNumColons = 2;

static constexpr PixelAddressType kNumPixels = 88;
static constexpr PixelAddressType kNumPixelsDigits = 84;
static constexpr PixelAddressType kNumPixelsColons = 4;
static constexpr PixelAddressType kNumPixelsPerSegment = 6;
static constexpr PixelAddressType kNumPixelsPerDigit = kNumPixelsPerSegment * 7;
static constexpr PixelAddressType kNumPixelsPerColon = 2;

#define SEVEN_SEGMENT_DIGITS_TRANSLATION_TABLE 0, 1, \
    2, 3, \
    6, 7, \
    8, 9, \
    10, 11, \
    12, 13, \
    4, 5, \
    14, 15, \
    16, 17, \
    20, 21, \
    22, 23, \
    24, 25, \
    26, 27, \
    18, 19, \
    30, 31, \
    32, 33, \
    36, 37, \
    38, 39, \
    40, 41, \
    42, 43, \
    34, 35, \
    44, 45, \
    46, 47, \
    50, 51, \
    52, 53, \
    54, 55, \
    56, 57, \
    48, 49, \
    60, 61, \
    62, 63, \
    66, 67, \
    68, 69, \
    70, 71, \
    72, 73, \
    64, 65, \
    74, 75, \
    76, 77, \
    80, 81, \
    82, 83, \
    84, 85, \
    86, 87, \
    78, 79

#define SEVEN_SEGMENT_COLON_TRANSLATION_TABLE 28, \
    29, \
    58, \
    59

#define SEVEN_SEGMENT_PIXEL_ANIMATION_ORDER 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13

