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

void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    HardwarePin &pin = *reinterpret_cast<HardwarePin *>(arg);
    pin._micros = micros();
    pin._intCount++;
    pin._value = digitalRead(pin._pin);
}

#if DEBUG
void Pin::dumpConfig(Print &output)
{
    output.printf_P(PSTR("pin=%u"), _pin);
}
#endif

Debounce *HardwarePin::getDebounce() const
{
    return nullptr;
}
