/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"

// Monitors attached pins and sends state after debouncing

namespace PinMonitor {

    class Monitor
    {
    public:
        Monitor();
        ~Monitor();

        // set debounce time, default = 10000 microseconds
        void setDebounceTime(uint32_t debounceTime) {
            _debounceTime = debounceTime;
        }

        // set default pin mode for adding new pins
        void setDefaultPinMode(uint8_t mode) {
            _pinMode = mode;
        }

        void begin();
        void end();

        // enable debug output on serial port
        void beginDebug();
        // disable debug mode
        void endDebug();

        // add a pin object
        Pin &attach(Pin *pin);

        // remove a pin or multiple pins
        // pinMode will be set to INPUT if the pin is not in use anymore
        void detach(Predicate pred);
        void detach(Iterator begin, Iterator end);
        void detach(Pin *pin);

    public:
        static void loop();
        static void callback(InterruptInfo info);

    private:
        Pin &_attach(Pin &pin);
        void _attachLoop();
        void _detachLoop();
        void _loop();
        void _callback(InterruptInfo info);
        void _event(uint8_t pin, bool state, TimeType now);

        uint32_t _debounceTime;

        Vector _pins;
        std::vector<PinUsage> _pinsActive;
        uint8_t _pinMode;
    };

}

extern PinMonitor::Monitor pinMonitor;
