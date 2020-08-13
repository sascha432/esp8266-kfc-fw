/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_IOT_BLINDS_CTRL
#define DEBUG_IOT_BLINDS_CTRL               0
#endif

// save last state on SPIFFS
#ifndef IOT_BLINDS_CTRL_SAVE_STATE
#define IOT_BLINDS_CTRL_SAVE_STATE          1
#endif

#ifndef IOT_BLINDS_CTRL_CHANNEL_COUNT
#define IOT_BLINDS_CTRL_CHANNEL_COUNT       2
#endif
#if IOT_BLINDS_CTRL_CHANNEL_COUNT != 2
#error 2 channels supported only
#endif

// enable RPM sensing, 0 = disable
#ifndef IOT_BLINDS_CTRL_RPM_PIN
#define IOT_BLINDS_CTRL_RPM_PIN             0
#endif

// number of pulses per rotation
#ifndef IOT_BLINDS_CTRL_RPM_PULSES
#define IOT_BLINDS_CTRL_RPM_PULSES          3
#endif

// AT mode command for parameter tuning
#ifndef IOT_BLINDS_CTRL_TESTMODE
#define IOT_BLINDS_CTRL_TESTMODE            1
#endif

// motor pins
#ifndef IOT_BLINDS_CTRL_M1_PIN
#define IOT_BLINDS_CTRL_M1_PIN              D1
#endif

#ifndef IOT_BLINDS_CTRL_M2_PIN
#define IOT_BLINDS_CTRL_M2_PIN              D2
#endif

#ifndef IOT_BLINDS_CTRL_M3_PIN
#define IOT_BLINDS_CTRL_M3_PIN              D6
#endif

#ifndef IOT_BLINDS_CTRL_M4_PIN
#define IOT_BLINDS_CTRL_M4_PIN              D7
#endif

// pin for the shunt multiplexer
#ifndef IOT_BLINDS_CTRL_RSSEL_PIN
#define IOT_BLINDS_CTRL_RSSEL_PIN           D5
#endif

// wait for RC filter to discharge, max. time in milliseconds
#ifndef IOT_BLINDS_CTRL_RSSEL_WAIT
#define IOT_BLINDS_CTRL_RSSEL_WAIT          50
#endif

// shunt resistance in milliohm
#ifndef IOT_BLINDS_CTRL_SHUNT
#define IOT_BLINDS_CTRL_SHUNT               100
#endif

namespace BlindsControllerConversion {

    static constexpr double kShuntValue = IOT_BLINDS_CTRL_SHUNT / 1000.0; // Ohm

    static constexpr int kADCMaxValue = 1024; // 0-1024 (!)
    // Function Measure the input voltage of TOUT pin 6; unit: 1/1024 V.
    // Prototype uint16	system_adc_read(void)
    static constexpr double kADCMaxVoltage = (1.0 / 1024.0) * kADCMaxValue;

    // ADC value = (kADCMaxValue * I * Rs) / kADCMaxVoltage
    static constexpr double kConvertCurrentToADCValueMulitplierAmps = (kADCMaxValue * kShuntValue) / kADCMaxVoltage;

    static constexpr float kConvertCurrentToADCValueMulitplier = kConvertCurrentToADCValueMulitplierAmps / 1000.0; // mA
    static constexpr float kConvertADCValueToCurrentMulitplier = 1000.0 / kConvertCurrentToADCValueMulitplierAmps;
    static constexpr uint16_t kMinCurrent = (0 * kConvertADCValueToCurrentMulitplier);
    static constexpr uint32_t __kMaxCurrent = (kADCMaxValue * kConvertADCValueToCurrentMulitplier);
    static constexpr uint16_t kMaxCurrent = __kMaxCurrent;

    static_assert(__kMaxCurrent == kMaxCurrent, "max. current cannot be stored in uint16_t");
    static_assert((__kMaxCurrent / 1000.0) < 12.0, "the ADC range is over 12A and a presicion will suffer");

}
