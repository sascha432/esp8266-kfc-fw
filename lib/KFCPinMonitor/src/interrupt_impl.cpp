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

#if PIN_MONITOR_USE_GPIO_INTERRUPT == 0 && PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0
#endif
#include <interrupts.h>

namespace PinMonitor {

    Interrupt::EventBuffer eventBuffer;
    uint16_t interrupt_levels;

// ------------------------------------------------------------------------
// implementation with GPIO interrupt instead of attachInterrupt...
// ------------------------------------------------------------------------
#if PIN_MONITOR_USE_GPIO_INTERRUPT


    void GPIOInterruptsEnable()
    {
        // #define GPCI   7  //INT_TYPE (3bits) 0:disable,1:rising,2:falling,3:change,4:low,5:high

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
        // ETS_GPIO_INTR_DISABLE();
        noInterrupts();
        for(auto pin: kPins) {
            // if ((status & PinAndMask(pin).mask) | ((PinMonitor::interrupt_levels & PinAndMask(pin).mask) ^ (levels & PinAndMask(pin).mask))) { // 842 free IRAM
            if ((status & PinAndMask(pin).mask) && (static_cast<bool>(PinMonitor::interrupt_levels & PinAndMask(pin).mask) != static_cast<bool>(levels &  PinAndMask(pin).mask))) { // 826 free IRAM
            // if (status & PinAndMask(pin).mask) {
                PinMonitor::eventBuffer.emplace_back(micros(), pin, levels);
            }
        }
        PinMonitor::interrupt_levels = levels;
        interrupts();
        // ETS_GPIO_INTR_ENABLE();
    }

#elif PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0

    typedef struct {
        voidFuncPtrArg fn;
        void *arg;
    } interrupt_handler_t;

    static interrupt_handler_t interrupt_handlers[15];
    static uint32_t interrupt_reg;

    static void set_interrupt_handlers(uint8_t pin, voidFuncPtrArg userFunc, void* arg)
    {
        interrupt_handlers[pin].fn = userFunc;
        interrupt_handlers[pin].arg = arg;
    }

    void ICACHE_RAM_ATTR interrupt_handler(void *)
    {
        uint32_t status = GPIE;
        GPIEC = status; // clear interrupts
        if (status == 0 || interrupt_reg == 0) {
            return;
        }
        noInterrupts();
        // ETS_GPIO_INTR_DISABLE();
        uint8_t i = 0;
        uint32_t changedbits = status & interrupt_reg; // remove bits that do not have an active handler otherwise it results in a nullptr call
        while (changedbits) {
            while (!(changedbits & (1 << i))) { // find next set bit
                i++;
            }
            changedbits &= ~(1 << i);
            interrupt_handlers[i].fn(interrupt_handlers[i].arg);
        }
        interrupts();
        // ETS_GPIO_INTR_ENABLE();
    }

    // mode is CHANGE
    void attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg)
    {
        if (userFunc == nullptr) { // detach interrupt if userfunc is nullptr. makes the check for nullptr inside the ISR obsolete
            detachInterrupt(pin);
            return;
        }
        ETS_GPIO_INTR_DISABLE();
        set_interrupt_handlers(pin, userFunc, arg);
        interrupt_reg |= (1 << pin);
        GPC(pin) &= ~(0xF << GPCI);  // INT mode disabled
        GPIEC = (1 << pin);  // Clear Interrupt for this pin
        GPC(pin) |= ((CHANGE & 0xF) << GPCI);  // INT mode "mode"
        ETS_GPIO_INTR_ATTACH(interrupt_handler, &interrupt_reg);
        ETS_GPIO_INTR_ENABLE();
    }

    // this function must not be called from inside the interrupt handler
    void detachInterrupt(uint8_t pin)
    {
        ETS_GPIO_INTR_DISABLE();
        GPC(pin) &= ~(0xF << GPCI);  // INT mode disabled
        GPIEC = (1 << pin);  // Clear Interrupt for this pin
        interrupt_reg &= ~(1 << pin);
        set_interrupt_handlers(pin, nullptr, nullptr);
        if (interrupt_reg) {
            ETS_GPIO_INTR_ENABLE();
        }
    }

#endif

}
