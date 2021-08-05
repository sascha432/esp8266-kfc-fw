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

            // uint32_t getStates() const;

        private:
            uint32_t _getGPIOStates() const;
            uint32_t _states;

            #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                uint16_t _getIOExpanderStates() const;
                uint16_t _expanderStates;
            #endif
        };

        inline PollingTimer::PollingTimer() : OSTimer(OSTIMER_NAME("PinMonitor::PollingTimer")), _states(0)
        {
        }

        inline uint32_t PollingTimer::_getGPIOStates() const
        {
            return GPI | ((GP16I & 0x01) << 16);
        }

    #endif

    #if PIN_MONITOR_USE_POLLING
        extern PollingTimer pollingTimer;
    #endif

}
