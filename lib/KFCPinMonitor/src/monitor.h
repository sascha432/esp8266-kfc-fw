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

        void setDebounceTime(uint8_t debounceTime);
        uint8_t getDebounceTime() const;

        // set default pin mode for adding new pins
        void setDefaultPinMode(uint8_t mode);

        // if the main loop is not executed fast enough, set useTimer to true
        void begin(bool useTimer = false);
        void end();

#if DEBUG
        // enable debug output on serial port
        void beginDebug(Print &outout, uint32_t interval = 1000);
        // disable debug mode
        void endDebug();

        bool isDebugRunning() const {
            return !!_debugTimer;
        }
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
        void detach(const void *arg);

        void detach(Iterator begin, Iterator end);
        void detach(Pin &pin);
        const Vector &getHandlers() const;
        const PinVector &getPins() const;

    public:
        static void loop();
        static void loopTimer(Event::CallbackTimerPtr);
        // return true if the loop has executed since the last call
        static bool getLoopExecuted();

        static const __FlashStringHelper *stateType2String(StateType state);
        static const __FlashStringHelper *stateType2Level(StateType state);

    private:
        Pin &_attach(Pin &pin);
        void _detach(Iterator begin, Iterator end, bool clear);
        void _attachLoop();
        void _detachLoop();
        void _loop();
        void _event(uint8_t pin, StateType state, uint32_t now);

    private:
        Vector _handlers;       // button handler, base class Pin
        PinVector _pins;        // pins class HardwarePin
        uint32_t _lastRun;
        Event::Timer *_loopTimer;
#if DEBUG
        Event::Timer *_debugTimer;
#endif

        uint8_t _pinMode;
        uint8_t _debounceTime;
    };

    inline void Monitor::setDebounceTime(uint8_t debounceTime)
    {
        _debounceTime = debounceTime;
    }

    inline uint8_t Monitor::getDebounceTime() const
    {
        return _debounceTime;
    }

    // set default pin mode for adding new pins
    inline void Monitor::setDefaultPinMode(uint8_t mode)
    {
        _pinMode = mode;
    }

    inline void Monitor::detach(Iterator begin, Iterator end)
    {
        _detach(begin, end, false);
    }

    inline void Monitor::detach(Pin &pin)
    {
        detach(&pin);
    }

    inline const Vector &Monitor::getHandlers() const
    {
        return _handlers;
    }

    inline const PinVector &Monitor::getPins() const
    {
        return _pins;
    }

}

extern PinMonitor::Monitor pinMonitor;
