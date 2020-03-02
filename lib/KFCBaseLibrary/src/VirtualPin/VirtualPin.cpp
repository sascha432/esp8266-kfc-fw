/**
  Author: sascha_lammers@gmx.de
*/

#ifndef ESP32

#include "VirtualPin.h"

#if DEBUG_VIRTUAL_PIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DEFINE_ENUM_BITSET(VirtualPinMode);

#endif
