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


// void SimpleHardwarePin::addEvent(uint32_t time, bool value)
// {
//     noInterrupts();
//     eventBuffer.emplace_back(time, _pin, value);
//     interrupts();
// }
