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

        inline void setDebounceTime(uint8_t debounceTime) {
            _debounceTime = debounceTime;
        }
        inline uint8_t getDebounceTime() const {
            return _debounceTime;
        }

        // set default pin mode for adding new pins
        inline void setDefaultPinMode(uint8_t mode) {
            _pinMode = mode;
        }

        // if the main loop is not executed fast enough, set useTimer to true
        void begin(bool useTimer = false);
        void end();

#if DEBUG
        // enable debug output on serial port
        void beginDebug(Print &outout, uint32_t interval = 1000);
        // disable debug mode
        void endDebug();
#endif

        // add a pin object
        Pin &attach(Pin *pin);

        template<class T, class ...Args>
        T &attach(Args&&... args) {
            return static_cast<T &>(attach(new T(std::forward<Args>(args)...)));
        }

        // remove one or more pins
        void detach(Predicate pred);
        void detach(Pin *pin);
        void detach(void *arg);

        inline void detach(Iterator begin, Iterator end) {
            _detach(begin, end, false);
        }
        inline void detach(Pin &pin) {
            detach(&pin);
        }

        const Vector &getVector() const {
            return _handlers;
        }

    public:
        static void loop();
        static void loopTimer(Event::CallbackTimerPtr);
        // return true if the loop has executed since the last call
        static bool getLoopExecuted();

        static const __FlashStringHelper *stateType2String(StateType);

    private:
        Pin &_attach(Pin &pin);
        void _detach(Iterator begin, Iterator end, bool clear);
        void _attachLoop();
        void _detachLoop();
        void _loop();
        void _event(uint8_t pin, StateType state, TimeType now);

    private:
        Vector _handlers;
        PinVector _pins;
        TimeType _lastRun;
        Event::Timer *_loopTimer;
#if DEBUG
        Event::Timer *_debugTimer;
#endif

        uint8_t _pinMode;
        uint8_t _debounceTime;
    };

}

extern PinMonitor::Monitor pinMonitor;
