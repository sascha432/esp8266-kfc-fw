/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "polling_timer.h"

namespace PinMonitor {

    // lock GPIO interrupts and polling timer
    struct GPIOInterruptLock {
        GPIOInterruptLock() :
            #if PIN_MONITOR_USE_POLLING
                _timer(pollingTimer),
            #endif
            _gpioState(gpioIntrEnabled())
        {
            lock();
        }
        ~GPIOInterruptLock() {
            unlock();
        }
        bool gpioIntrEnabled() const {
            uint32_t state;
            __asm__ __volatile__ ("rsr %0,ps":"=a" (state));
            return state & _BV(ETS_GPIO_INUM);
        }
        void lock() {
            if (_gpioState) {
                ETS_GPIO_INTR_DISABLE();
                ::printf(".");
            }
            if (_timer) {
                pollingTimer.disable();
                ::printf(",");
            }
        }
        void unlock() {
            if (_gpioState) {
                ETS_GPIO_INTR_ENABLE();
                ::printf(":");
            }
            if (_timer) {
                pollingTimer.enable();
                ::printf(";");
            }
        }
        #if PIN_MONITOR_USE_POLLING
            bool _timer;
        #endif
        bool _gpioState;
    };

}
