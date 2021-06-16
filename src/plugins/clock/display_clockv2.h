// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: ./scripts/tools/create_7segment_display.py dumpcode --name clockv2

#pragma once

#include <Arduino_compat.h>
#include <stdint.h>
#include <array>

using PixelAddressType = uint8_t;
using PixelAddressPtr = const PixelAddressType *;

inline static PixelAddressType readPixelAddress(PixelAddressPtr ptr) {
    return pgm_read_byte(ptr);
}

static constexpr PixelAddressType kNumDigits = 4;
static constexpr PixelAddressType kNumColons = 1;

static constexpr PixelAddressType kNumPixels = 116;
static constexpr PixelAddressType kNumPixelsDigits = 112;
static constexpr PixelAddressType kNumPixelsColons = 4;
static constexpr PixelAddressType kNumPixelsPerSegment = 4;
static constexpr PixelAddressType kNumPixelsPerDigit = kNumPixelsPerSegment * 7;
static constexpr PixelAddressType kNumPixelsPerColon = 2;

#define SEVEN_SEGMENT_DIGITS_TRANSLATION_TABLE 112, 113, 114, 115, \
    108, 109, 110, 111, \
    96, 97, 98, 99, \
    100, 101, 102, 103, \
    104, 105, 106, 107, \
    88, 89, 90, 91, \
    92, 93, 94, 95, \
    84, 85, 86, 87, \
    80, 81, 82, 83, \
    68, 69, 70, 71, \
    72, 73, 74, 75, \
    76, 77, 78, 79, \
    60, 61, 62, 63, \
    64, 65, 66, 67, \
    28, 29, 30, 31, \
    24, 25, 26, 27, \
    12, 13, 14, 15, \
    16, 17, 18, 19, \
    20, 21, 22, 23, \
    4, 5, 6, 7, \
    8, 9, 10, 11, \
    56, 57, 58, 59, \
    52, 53, 54, 55, \
    40, 41, 42, 43, \
    44, 45, 46, 47, \
    48, 49, 50, 51, \
    32, 33, 34, 35, \
    36, 37, 38, 39

#define SEVEN_SEGMENT_COLONTRANSLATIONTABLE 0, 1, \
    2, 3

#define SEVEN_SEGMENT_PIXEL_ANIMATION_ORDER 28, 29, 30, 31, 24, 25, 26, 27, 11, 10, 9, 8, 20, 21, 22, 23, 16, 17, 18, 19, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4

