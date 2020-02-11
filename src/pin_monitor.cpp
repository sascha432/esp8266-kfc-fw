/**
  Author: sascha_lammers@gmx.de
*/

#if PIN_MONITOR

#include "pin_monitor.h"
#include <EventScheduler.h>
#include "plugins.h"
#include <FunctionalInterrupt.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PinMonitor *PinMonitor::_instance = nullptr;

PinMonitor::PinMonitor() {
}

PinMonitor::~PinMonitor()
{
    for(auto &pin: _pins) {
        detachInterrupt(digitalPinToInterrupt(pin.getPin()));
    }
}

PinMonitor *PinMonitor::createInstance()
{
    if (!_instance) {
        _instance = new PinMonitor();
    }
    return _instance;
}

void PinMonitor::deleteInstance()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

void PinMonitor::callback(InterruptInfo info)
{
    _debug_printf_P(PSTR("PinMonitor::callback(): pin=%u,value=%u,micros=%u\n"), info.pin, info.value, info.micro);
    auto monitor = PinMonitor::getInstance();
    if (monitor) {
        for(auto &pin: monitor->_pins) {
            if (pin.getPin() == info.pin) {
                pin.invoke(info.value, info.micro);
            }
        }
    }
}

PinMonitor::Pin *PinMonitor::addPin(uint8_t pin, Callback_t callback, void *arg, uint8_t mode)
{
    if (_findPin(pin, arg) == _pins.end()) {
        _debug_printf_P(PSTR("PinMonitor::addPin(): adding pin %u, mode %u, callback %p, arg %p\n"), pin, mode, callback, arg);
        _pins.emplace_back(Pin(pin, digitalRead(pin), callback, arg));
        attachScheduledInterrupt(digitalPinToInterrupt(pin), PinMonitor::callback, CHANGE);
        pinMode(pin, mode);
        return &_pins.back();
    }
    return nullptr;

}

bool PinMonitor::removePin(uint8_t pin, void *arg)
{
    auto _pin = _findPin(pin, arg);
    if (_pin == _pins.end()) {
        return false;
    }
    _debug_printf_P(PSTR("PinMonitor::removePin(): removing pin %u, callback %p, arg %p\n"), pin, _pin->_callback, _pin->_arg);
    detachInterrupt(digitalPinToInterrupt(pin));
    _pins.erase(_pin);
    return true;
}

void PinMonitor::dumpPins(Stream &output)
{
    output.printf_P(PSTR("+PINM count=%u\n"), _pins.size());
    if (_pins.size()) {
        for(auto &pin: _pins) {
            output.printf_P(PSTR("+PINM pin=%u value=%d micro=%u callback=%p arg=%p\n"), pin._pin, pin._value, pin._micro, pin._callback, pin._arg);
        }
    }
}

PinMonitor::PinsVectorIterator PinMonitor::_findPin(uint8_t pin, void *arg)
{
    return std::find_if(_pins.begin(), _pins.end(), [pin, arg](const Pin &_pin) {
        return (_pin.getPin() == pin) && (!arg || _pin.getArg() == arg);
    });
}

#endif
