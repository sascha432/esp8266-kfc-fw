/**
 * Author: sascha_lammers@gmx.de
 */

#if STK500V1

//TODO not working yet

// STK500v1 programmer plugin
// fully asynchronous

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_STK500V1
#define DEBUG_STK500V1 0
#endif

#if !AT_MODE_SUPPORTED
#error Requires AT_MODE_SUPPORTED=1
#endif

#ifndef STK500V1_SERIAL
#error Serial port not defined
#endif
#ifndef STK500V1_RESET_PIN
#error Reset pin not defined
#endif

#endif
