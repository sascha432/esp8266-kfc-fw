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
#define IOT_BLINDS_CTRL_M1_PIN              D2
#endif

#ifndef IOT_BLINDS_CTRL_M2_PIN
#define IOT_BLINDS_CTRL_M2_PIN              D1
#endif

#ifndef IOT_BLINDS_CTRL_M3_PIN
#define IOT_BLINDS_CTRL_M3_PIN              D7
#endif

#ifndef IOT_BLINDS_CTRL_M4_PIN
#define IOT_BLINDS_CTRL_M4_PIN              D6
#endif

// PWM frequency in Hz
#ifndef IOT_BLINDS_CTRL_PWM_FREQ
#define IOT_BLINDS_CTRL_PWM_FREQ            17500
#endif

// pin for the shunt multiplexer
#ifndef IOT_BLINDS_CTRL_RSSEL_PIN
#define IOT_BLINDS_CTRL_RSSEL_PIN           D5
#endif

// delay in milliseconds after switching
#ifndef IOT_BLINDS_CTRL_RSSEL_WAIT
#define IOT_BLINDS_CTRL_RSSEL_WAIT          5
#endif

// shunt resistance in milliohm
#ifndef IOT_BLINDS_CTRL_SHUNT
#define IOT_BLINDS_CTRL_SHUNT               125UL
#endif

// time divider for the ADC integration to get a stable current reading
// depends on the PWM freqency and capacitor on the control board
#ifndef IOT_BLINDS_CTRL_INT_TDIV
#define IOT_BLINDS_CTRL_INT_TDIV            25
#endif

// max. ADC voltage in millivolt
#ifndef IOT_BLINDS_CTRL_ADCV
// NodeMCU A0, which uses a resistor divider that needs a correction factor
// #define IOT_BLINDS_CTRL_ADCV        (unsigned long)(3300 / 2.1)
// directly connected to the ADC
#define IOT_BLINDS_CTRL_ADCV                1000UL
#endif

// ADC=(1024*I*RS)/V
// ADC=(1024*(mA/1000)*(IOT_BLINDS_CTRL_SHUNT/1000))/(IOT_BLINDS_CTRL_ADCV/1000)
#define CURRENT_TO_ADC(value)               (uint32_t)round((128UL * IOT_BLINDS_CTRL_SHUNT * value) / (double)(125UL * IOT_BLINDS_CTRL_ADCV))

// I=(ADC/1024)*V/RS
// mA=((value/1024)*(IOT_BLINDS_CTRL_ADCV/1000)/(IOT_BLINDS_CTRL_SHUNT/1000))*1000
#define ADC_TO_CURRENT(value)               (uint32_t)round((IOT_BLINDS_CTRL_ADCV * value * 1000UL) / (double)(IOT_BLINDS_CTRL_SHUNT * 1024UL))
