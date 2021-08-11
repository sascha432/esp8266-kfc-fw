/**
 * Author: sascha_lammers@gmx.de
 */

// STK500v1 programmer plugin
// fully asynchronous implementation
// http://www.amelek.gda.pl/avr/uisp/doc2525.pdf

// at command
// +stk500v1f=/firmware.hex,0,4
//            ^^^^^^^^ the hex file must be on the file system
//                          ^ 0=Serial/1=Serial1
//                            ^ Write debug outout to /stk500v1/debug.log (for more options see LoggingEnum_t)
// LED blinking: trying to sync with MCU
// LED flashing: writing or reading data

// +stk500v1s=atmega328p
// +STK500V1S: Signature set
// +stk500v1f=/firmware.hex
// +STK500V1F: Flashing /firmware.hex on Serial
// Input file validated. 22846 bytes to write...
// Connected to bootloader...
// Device signature = 0x1e950f

// Writing: | ################################################## | Complete

// Reading: | ################################################## | Complete

// 22846 bytes verified
// Programming successful
// Done

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_STK500V1
#    define DEBUG_STK500V1 0
#endif

#if !AT_MODE_SUPPORTED
#    error Requires AT_MODE_SUPPORTED=1
#endif

#ifndef STK500V1_RESET_PIN
#    error Reset pin not defined
#endif

#ifndef STK500_HAVE_SOFTWARE_SERIAL
#    define STK500_HAVE_SOFTWARE_SERIAL 0
#endif

#ifndef STK500_HAVE_SOFTWARE_SERIAL_PINS
#    define STK500_HAVE_SOFTWARE_SERIAL_PINS D5, D6
#endif

#ifndef STK500_HAVE_FUSES
#    define STK500_HAVE_FUSES 0
#endif
