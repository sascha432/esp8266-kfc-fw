/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "OSTimer.h"

// Monitors attached pins and sends state after debouncing

namespace PinMonitor {

    #if PIN_MONITOR_USE_POLLING

        class PollingTimer : public OSTimer {
        public:
            PollingTimer();

            void start();
            virtual void run();

            void disable();
            void enable();

            uint16_t getStates() const;

        private:
            uint16_t _states;
            #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                uint16_t _expanderStates;
            #endif
        };

        inline PollingTimer::PollingTimer() : OSTimer(OSTIMER_NAME("PinMonitor::PollingTimer")), _states(0)
        {
        }

        inline uint16_t PollingTimer::getStates() const
        {
            return _states;
        }

    #endif

    #if PIN_MONITOR_USE_POLLING
        extern PollingTimer pollingTimer;
    #endif

}
