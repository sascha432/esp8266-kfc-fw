/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "debounce.h"

using namespace PinMonitor;

PinUsage::PinUsage(uint8_t pin, uint16_t debounceTime) :
    _pin(pin),
    _count(0),
    _debounce(new Debounce(debounceTime))
{

}

Debounce &PinUsage::getDebounce()
{
    return *_debounce.get();
}
