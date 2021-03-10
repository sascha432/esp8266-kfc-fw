/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include "interrupt_impl.h"
#include "rotary_encoder.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

namespace PinMonitor {

    Interrupt::EventBuffer eventBuffer;
    Interrupt::LookupTable lookupTable;
    uint16_t interrupt_levels;

#if PIN_MONITOR_USE_GPIO_INTERRUPT
// ------------------------------------------------------------------------
// implementation with GPIO interrupt
// ------------------------------------------------------------------------

    void GPIOInterruptsEnable()
    {
        ETS_GPIO_INTR_DISABLE();
        eventBuffer.clear();
        for(auto pin: Interrupt::kPins) {
            GPC(pin) &= ~(0xF << GPCI);  // INT mode disabled
            GPIEC = (1 << pin);  // Clear Interrupt for this pin
            GPC(pin) |= ((CHANGE & 0xF) << GPCI);  // INT mode "mode"
        }
        ETS_GPIO_INTR_ATTACH(pin_monitor_interrupt_handler, nullptr);
        interrupt_levels = GPI;
        ETS_GPIO_INTR_ENABLE();
    }

    void GPIOInterruptsDisable()
    {
        ETS_GPIO_INTR_DISABLE();
    }

}

inline static void __GPIOInterruptsBegin() {
    using namespace PinMonitor;
    GPIOInterruptsEnable();
}

void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr)
{
    using namespace PinMonitor::Interrupt;
    uint32_t status = GPIE;
    GPIEC = status;
    if ((status & kPinMask) == 0) {
        return;
    }
    auto levels = static_cast<uint16_t>(GPI);

    // to keep the code simple and small in here, just save the time and the GPIO input state
    ETS_GPIO_INTR_DISABLE();
    for(auto pin: kPins) {
        uint16_t mask = _BV(pin);
        if ((status & mask) && (static_cast<bool>(PinMonitor::interrupt_levels & mask) != static_cast<bool>(levels & mask))) {
            PinMonitor::eventBuffer.emplace_back(micros(), pin, levels);
        }
    }
    PinMonitor::interrupt_levels = levels;
    ETS_GPIO_INTR_ENABLE();
}

// extern "C" void initVariant()
// {
//     __GPIOInterruptsBegin();
// }

#endif

