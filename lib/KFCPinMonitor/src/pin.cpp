/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "pin.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

#if DEBUG
void Pin::dumpConfig(Print &output)
{
    output.printf_P(PSTR("pin=%u"), _pin);
}
#endif
