/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_CLOCK
#define DEBUG_IOT_CLOCK                         1
#endif

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "SevenSegmentPixel.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#ifndef IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#define IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL         1
#endif

#if IOT_LED_MATRIX

#define IOT_CLOCK_NUM_PIXELS                        (IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS)

// first pixel to use, others can be controlled separately and are reset during reboot only
#ifndef IOT_LED_MATRIX_START_ADDR
#define IOT_LED_MATRIX_START_ADDR                   0
#endif

#else


// number of digits
#ifndef IOT_CLOCK_NUM_DIGITS
#define IOT_CLOCK_NUM_DIGITS                        4
#endif

// pixels per segment
#ifndef IOT_CLOCK_NUM_PIXELS
#define IOT_CLOCK_NUM_PIXELS                        2
#endif

#ifndef IOT_LED_MATRIX_COLS
#define IOT_LED_MATRIX_COLS                         IOT_CLOCK_NUM_PIXELS
#define IOT_LED_MATRIX_ROWS                         1
#endif

// number of colons
#ifndef IOT_CLOCK_NUM_COLONS
#if IOT_CLOCK_NUM_DIGITS == 4
#define IOT_CLOCK_NUM_COLONS                        1
#else
#define IOT_CLOCK_NUM_COLONS                        2
#endif
#endif

// pixels per dot
#ifndef IOT_CLOCK_NUM_COLON_PIXELS
#define IOT_CLOCK_NUM_COLON_PIXELS                  2
#endif

// order of the segments (a-g)
#ifndef IOT_CLOCK_SEGMENT_ORDER
#define IOT_CLOCK_SEGMENT_ORDER                     { 0, 1, 3, 4, 5, 6, 2 }
#endif

// digit order, 30=colon #1,31=#2, etc...
#ifndef IOT_CLOCK_DIGIT_ORDER
#define IOT_CLOCK_DIGIT_ORDER                       { 0, 1, 30, 2, 3, 31, 4, 5 }
#endif

// pixel order of pixels that belong to digits, 0 to use pixel addresses of the 7 segment class
#ifndef IOT_CLOCK_PIXEL_ORDER
#define IOT_CLOCK_PIXEL_ORDER                       { 0 }
#endif

#endif

class ClockPlugin;

using KFCConfigurationClasses::Plugins;

namespace Clock {

    using Config_t = Plugins::Clock::ClockConfig_t;
    using ColorType = uint32_t;
    using ClockColor_t = Plugins::Clock::ClockColor_t;
    using AnimationType = Config_t::AnimationType;
    using InitialStateType = Config_t::InitialStateType;

    // static constexpr auto kTotalPixelCount = SevenSegmentDisplay::kTotalPixelCount;
    static constexpr uint8_t kMaxBrightness = 0xff;
    static constexpr uint8_t kUpdateRate = 20;
    static constexpr uint8_t kBrightness75 = kMaxBrightness * 0.75;
    static constexpr uint8_t kBrightness50 = kMaxBrightness * 0.5;
    static constexpr uint8_t kBrightnessTempProtection = kMaxBrightness * 0.25;

    template<class _CoordinateType, _CoordinateType _Rows, _CoordinateType _Columns> class PixelCoordinates;

    template<size_t _StartAddress, size_t _Rows, size_t _Columns>
    class Types {
    protected:
        static constexpr size_t kTotalPixelCount = _Rows * _Columns + _StartAddress;

    private:
        static constexpr size_t _kCols = _Columns;
        static constexpr size_t _kRows = _Rows;

    public:
        using PixelAddressType = typename std::conditional<(kTotalPixelCount > 255), uint16_t, uint8_t>::type;
        using CoordinateType = typename std::conditional<(_kRows > 255 || _kCols > 255), uint16_t, uint8_t>::type;
        using PixelCoordinatesType = PixelCoordinates<CoordinateType, _Rows, _Columns>;

    public:
        static constexpr CoordinateType kCols = _Columns;
        static constexpr CoordinateType kRows = _Rows;
        static constexpr PixelAddressType kStartAddress = _StartAddress;
        static constexpr PixelAddressType kNumPixels = kRows * kCols;
    };
}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
