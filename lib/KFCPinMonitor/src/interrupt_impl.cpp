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


namespace PinMonitor {

    Interrupt::EventBuffer eventBuffer;

// ------------------------------------------------------------------------
// implementation with GPIO interrupt instead of Arduino attachInterrupt..
// ------------------------------------------------------------------------
#if PIN_MONITOR_USE_GPIO_INTERRUPT

    uint16_t interrupt_levels;

    void GPIOInterruptsEnable()
    {
        ETS_GPIO_INTR_DISABLE();
        eventBuffer.clear();
        for(auto pin: Interrupt::kPins) {
            GPC(pin) &= ~(0xF << GPCI);  // INT mode disabled
            GPC(pin) |= ((CHANGE & 0xF) << GPCI);  // INT mode "mode"
        }
        GPIEC = Interrupt::PinAndMask::mask_of(Interrupt::kPins);  // clear interrupts for all pins
        ETS_GPIO_INTR_ATTACH(pin_monitor_interrupt_handler, nullptr);
        interrupt_levels = GPI;
        ETS_GPIO_INTR_ENABLE();
    }

    void GPIOInterruptsDisable()
    {
        ETS_GPIO_INTR_DISABLE();
    }

    void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr)
    {
        using namespace PinMonitor::Interrupt;
        uint32_t status = GPIE;
        GPIEC = status;
        if ((status & PinAndMask::mask_of(kPins)) == 0) {
            return;
        }
        auto levels = static_cast<uint16_t>(GPI); // we skip GPIO16 since it cannot handle interrupts anyway

        // to keep the code simple and small in here, just save the time and the GPIO input state
        ETS_GPIO_INTR_DISABLE();
        for(auto pin: kPins) {
            if ((status & PinAndMask(pin).mask) | ((PinMonitor::interrupt_levels & PinAndMask(pin).mask) ^ (levels & PinAndMask(pin).mask))) { // 842 free IRAM
            // if ((status & mask) && (static_cast<bool>(PinMonitor::interrupt_levels & mask) != static_cast<bool>(levels &  mask))) { // 826 free IRAM
                PinMonitor::eventBuffer.emplace_back(micros(), pin, levels);
            }
        }
        PinMonitor::interrupt_levels = levels;
        ETS_GPIO_INTR_ENABLE();
    }

}

#endif
