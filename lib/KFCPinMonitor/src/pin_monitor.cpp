/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "pin.h"
#include "debounce.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    HardwarePin &pin = *reinterpret_cast<HardwarePin *>(arg);
    pin._micros = micros();
    pin._intCount++;
    pin._value = digitalRead(pin._pin);
}
