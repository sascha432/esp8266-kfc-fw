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

#if PIN_MONITOR_USE_GPIO_INTERRUPT == 0

void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    auto pinPtr = reinterpret_cast<HardwarePin *>(arg);
    switch(pinPtr->_type) {
#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON
        case HardwarePinType::DEBOUNCE:
            reinterpret_cast<DebouncedHardwarePin *>(arg)->addEvent(micros(), GPI & _BV(pinPtr->getPin()));
            break;
#endif
#if PIN_MONITOR_SIMPLE_PIN
        case HardwarePinType::SIMPLE:
            reinterpret_cast<SimpleHardwarePin *>(arg)->addEvent(GPI & _BV(pinPtr->getPin()));
            break;
#endif
#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
        case HardwarePinType::ROTARY:
            PinMonitor::eventBuffer.emplace_back(micros(), pinPtr->getPin());
            break;
#endif
        default:
            break;
    }
}

#endif
