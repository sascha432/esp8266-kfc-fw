/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "pin_monitor.h"
#include "pin.h"
#include "Schedule.h"
#include "interrupt_impl.h"
#include "rotary_encoder.h"
#include "monitor.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

#if PIN_MONITOR_USE_GPIO_INTERRUPT == 0 || PIN_MONITOR_USE_POLLING == 1

#if PIN_MONITOR_USE_POLLING == 1
void HardwarePin::callback(void *arg, uint16_t _GPI, uint16_t mask)
#else
#define _GPI GPI
#define mask _BV(pinPtr->getPin())
void IRAM_ATTR HardwarePin::callback(void *arg)
#endif
{
    auto pinPtr = reinterpret_cast<HardwarePin *>(arg);
#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON || PIN_MONITOR_ROTARY_ENCODER_SUPPORT
    auto _micros = micros();
#endif
    switch(pinPtr->_type) {
#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON
        case HardwarePinType::DEBOUNCE:
            reinterpret_cast<DebouncedHardwarePin *>(arg)->addEvent(_micros, _GPI & mask);
            break;
#endif
#if PIN_MONITOR_SIMPLE_PIN
        case HardwarePinType::SIMPLE:
            reinterpret_cast<SimpleHardwarePin *>(arg)->addEvent(_GPI & mask);
            break;
#endif
#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
        case HardwarePinType::ROTARY:
            PinMonitor::eventBuffer.emplace_back(_micros, pinPtr->getPin(), _GPI);
            break;
#endif
        default:
            break;
    }
}
#if PIN_MONITOR_USE_POLLING == 0
#undef _GPI
#undef mask
#endif

#endif
