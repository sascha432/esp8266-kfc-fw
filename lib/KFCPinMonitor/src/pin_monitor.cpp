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

// commented code not tested

// HardwarePin::HardwarePin(HardwarePin &&move) noexcept :
//     _pin(move._pin),
//     _count(move._count),
//     _debounce(std::exchange(move._debounce, nullptr)),
//     _micros(move._micros),
//     _intCount(move._intCount),
//     _value(move._value)
// {
// }

// #pragma push_macro("new")
// #undef new

// HardwarePin &HardwarePin::operator=(HardwarePin &&move) noexcept
// {
//     _debounce.reset();
//     ::new(static_cast<void *>(this)) HardwarePin(std::move(move));
//     return *this;
// }

// #pragma pop_macro("new")


