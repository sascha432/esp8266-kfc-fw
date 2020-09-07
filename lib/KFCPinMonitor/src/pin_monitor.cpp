/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "debounce.h"

using namespace PinMonitor;

void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    HardwarePin &pin = *reinterpret_cast<HardwarePin *>(arg);
    pin._micros = micros();
    pin._intCount++;
    pin._value = digitalRead(pin._pin);
}
