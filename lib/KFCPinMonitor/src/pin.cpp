/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "pin.h"
#include "rotary_encoder.h"
#include "Schedule.h"
#include "interrupt_impl.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;


void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    auto pin = reinterpret_cast<HardwarePin *>(arg)->getPin();
    PinMonitor::eventBuffer.emplace_back(micros(), pin, GPI);
}
