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

#if PIN_MONITOR
#    include <PinMonitor.h>
#    include <push_button.h>
#    include <rotary_encoder.h>
#endif

#define FASTLED_INTERNAL
#include <FastLED.h>
#if ESP32
#    include <platforms/esp/32/clockless_rmt_esp32.h>
#endif

class ClockPlugin;

namespace Clock {

    using namespace KFCConfigurationClasses::Plugins::ClockConfigNS;

    // using ConfigType = Plugins::Clock::ClockConfig_t;
    using ColorType = uint32_t;
    // using ClockColor_t = Plugins::Clock::ClockColor_t;
    // using AnimationType = ConfigType::AnimationType;
    // using InitialStateType = ConfigType::InitialStateType;

    // static constexpr auto kMaxPixelAddress = SevenSegmentDisplay::kMaxPixelAddress;
    static constexpr uint8_t kMaxBrightness = 0xff;
    static constexpr uint8_t kUpdateRate = 20;
    static constexpr uint8_t kBrightness75 = kMaxBrightness * 0.75;
    static constexpr uint8_t kBrightness50 = kMaxBrightness * 0.5;
    static constexpr uint8_t kBrightnessTempProtection = kMaxBrightness * 0.25;

    template<class _CoordinateType, _CoordinateType _Rows, _CoordinateType _Columns> class PixelCoordinates;

    template<size_t _PixelOffset, size_t _Rows, size_t _Columns>
    class Types {
    protected:
        static constexpr size_t kMaxPixelAddress = IOT_CLOCK_NUM_PIXELS + _PixelOffset;//(_Rows * _Columns) + _PixelOffset;

    private:
        static constexpr size_t _kCols = _Columns;
        static constexpr size_t _kRows = _Rows;
        static constexpr size_t _kPixelOffset = _PixelOffset;

    public:
        using PixelAddressType = uint16_t; //typename std::conditional<(kMaxPixelAddress > 255), uint16_t, uint8_t>::type;
        using CoordinateType = uint16_t; //typename std::conditional<(_kRows > 255 || _kCols > 255), uint16_t, uint8_t>::type;
        using PixelCoordinatesType = PixelCoordinates<CoordinateType, _Rows, _Columns>;

    public:
        static constexpr CoordinateType kCols = _Columns;
        static constexpr CoordinateType kRows = _Rows;
        static constexpr PixelAddressType kPixelOffset = _PixelOffset;
        static constexpr PixelAddressType kNumPixels = kMaxPixelAddress;
    };
}

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif
