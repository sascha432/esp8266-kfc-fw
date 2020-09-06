/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"

namespace PinMonitor {

    class Pin
    {
    public:
        // arg can be used to add an unique identifier
        Pin(ConfigType config, void *arg);
        virtual ~Pin();

        ConfigType &configure();

        // receives events with state true for active and false for inactive
        // if the pin is confiured as activeLow, the state is inverted
        // now is the time when trhe event occured
        virtual void event(bool state, TimeType now);

    public:
        uint8_t getPin() const {
            return _config.getPin();
        }
        bool getState() const {
            return _state;
        }
        void *getArg() const {
            return _arg;
        }

    private:
        friend Monitor;

        ConfigType _config;
        void *_arg;
        bool _state;
    };

}
