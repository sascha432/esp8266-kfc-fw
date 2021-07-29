/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"
#include "debug_helper_disable.h"

#if HAVE_IOEXPANDER

#ifndef IOEXPANDER_INLINE
#define IOEXPANDER_INLINE inline
#endif

// DDR high = output, low = input. PORT is unchanged
// PIN reads from the exapnder
// PORT writes to the expander if output it enabled

#include "PCF8574.hpp"
#include "TinyPwm.hpp"
// #include "MCP23017.hpp"
#include "PCA9685.hpp"

namespace IOExpander {

    // _EndPin = 0: auto mode
    // _EndPin is _BeginPin + pin count, _BeginPin <= PINS < _EndPin
    // template<typename _DeviceClassType, typename _DeviceType, uint8_t _Address, uint8_t _BeginPin, uint8_t _EndPin = 0>
    // struct DeviceConfig {
    //     using DeviceClassType = _DeviceClassType;
    //     using DeviceType = _DeviceType;

    //     static constexpr DeviceType kDeviceType = DeviceType::kDeviceType;
    //     static constexpr uint8_t kI2CAddress = _Address;
    //     static constexpr uint8_t kBeginPin = _BeginPin;
    //     static constexpr uint8_t kEndPin = _EndPin == 0 ? (kBeginPin + DeviceType::kNumPins) : _EndPin;
    //     static constexpr uint8_t kNumPins = kEndPin - kBeginPin;

    //     static_assert(_BeginPin >= kDigitalPinCount, "_BeginPin must be greater or eqal kDigitalPinCount");

    //     static constexpr bool pinMatch(uint8_t pin) {
    //         return pin >= kBeginPin && pin <= kEndPin;
    //     }
    // };

    #define ST <
    #define GT >

    extern ConfigIterator<IOEXPANDER_DEVICE_CONFIG> config;

    #undef ST
    #undef GT

}

#endif
