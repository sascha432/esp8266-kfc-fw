/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"

namespace PinMonitor {

    class Pin : public ConfigType
    {
    public:
        using StateType = PinMonitor::StateType;
        using TimeType = PinMonitor::TimeType;

        // arg can be used to add an unique identifier
        Pin(uint8_t pin, void *arg, StateType states = StateType::HIGH_LOW, bool activeLow = false) :
            ConfigType(pin, states, activeLow),
            _arg(arg),
            _eventCounter(0)
        {
        }

        // event handlers are not executed more than once per millisecond
        virtual void event(StateType state, TimeType now) {}

        // loop is called even if events are disabled
        virtual void loop() {}

        inline void *getArg() const {
            return _arg;
        }

        inline void setEnabled(bool enabled) {
            setDisabled(!enabled);
        }

    private:
        friend Monitor;

        void *_arg;
        uint32_t _eventCounter;
    };

}
