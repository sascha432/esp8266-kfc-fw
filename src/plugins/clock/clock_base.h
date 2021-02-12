/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <ReadADC.h>
#include "clock_def.h"

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if IOT_CLOCK_BUTTON_PIN != -1
#include <PinMonitor.h>
#include <push_button.h>
#include <rotary_encoder.h>
#endif

// #ifndef FASTLED_ALLOW_INTERRUPTS
// #define FASTLED_ALLOW_INTERRUPTS                                0
// #endif
// #ifndef FASTLED_INTERRUPT_RETRY_COUNT
// #define FASTLED_INTERRUPT_RETRY_COUNT                           1
// #endif
#define FASTLED_INTERNAL
#include <FastLED.h>

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
